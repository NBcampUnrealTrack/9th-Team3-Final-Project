"""
번역 시트 도구 — 언리얼 .po 파일과 번역 관리 시트(Knight_Localization.xlsx) 사이를 오간다.

사용법 (Tools/Localization/loc.bat 으로 실행하면 엔진에 들어 있는 Python을 쓴다):
    loc.bat gather   게임 문구를 다시 모으고(.po 갱신) 시트에 반영한다. 기존 번역·상태·메모는 남는다.
    loc.bat export   .po → 시트 반영만 한다(언리얼 수집 없이).
    loc.bat check    시트의 번역을 검사해 문제를 출력하고 시트의 '검사' 칸을 갱신한다.
    loc.bat import   시트 → .po로 쓰고, 언리얼에 가져오기(Import) + 컴파일(Compile)까지 한다.
    --no-unreal      gather/import에서 언리얼 명령은 건너뛴다.

시트 구성:
    '번역' 탭   한 줄 = 문구 하나. 언어마다 [번역, 상태] 두 칸.
                상태는 미번역 → 번역됨 → 검수됨 → 게임 확인됨. 원문이 바뀐 줄은 '원문 변경됨'으로 바뀐다.
                '맥락'(번역가에게 줄 설명)과 '최대 길이'(글자 수, 비우면 검사 안 함)는 사람이 채운다.
    '용어집' 탭 게임 용어의 언어별 고정 번역. 원문에 용어가 있는데 번역에 고정 번역이 없으면 검사에서 알린다.
    '요약' 탭   언어별 상태 개수(export/check 때마다 다시 계산).
    '보관' 탭   게임에서 사라진 문구의 번역. 키가 바뀌었을 때 옮겨 쓰려고 남겨 둔다.

외부 패키지 없이 xlsx를 직접 읽고 쓴다(엔진 Python에 openpyxl이 없어서).
엑셀·구글 시트에서 열고 저장해도 그대로 다시 읽을 수 있다.
"""

import argparse
import os
import re
import subprocess
import sys
import zipfile
from xml.etree import ElementTree as ET
from xml.sax.saxutils import escape

# ── 경로·언어 설정 ─────────────────────────────────────────────

TOOL_DIR = os.path.dirname(os.path.abspath(__file__))
PROJECT_DIR = os.path.normpath(os.path.join(TOOL_DIR, "..", ".."))
LOC_DIR = os.path.join(PROJECT_DIR, "Content", "Localization", "Game")
SHEET_PATH = os.path.join(TOOL_DIR, "Knight_Localization.xlsx")
UPROJECT = os.path.join(PROJECT_DIR, "knight.uproject")

NATIVE = "ko"
# 번역 언어. Config/Localization/Game_*.ini 와 DefaultEditor.ini 의 언어 목록과 같아야 한다.
CULTURES = ["en", "ja", "zh-Hans", "zh-Hant"]
CULTURE_NAMES = {"en": "영어", "ja": "일본어", "zh-Hans": "중국어 간체", "zh-Hant": "중국어 번체"}

ST_EMPTY, ST_DONE, ST_REVIEWED, ST_INGAME, ST_CHANGED = "미번역", "번역됨", "검수됨", "게임 확인됨", "원문 변경됨"
STATUSES = [ST_EMPTY, ST_DONE, ST_REVIEWED, ST_INGAME, ST_CHANGED]

# '번역' 탭 칸 순서. 앞 6칸은 고정, 그 뒤로 언어마다 2칸, 마지막에 공통 칸.
FIXED_COLS = ["ID", "위치", "원문(ko)", "맥락", "최대 길이", "길이(ko)"]
TAIL_COLS = ["검수자", "메모", "검사"]


def lang_cols(culture):
    return f"{culture} 번역", f"{culture} 상태"


def all_columns():
    cols = list(FIXED_COLS)
    for c in CULTURES:
        cols += list(lang_cols(c))
    return cols + TAIL_COLS


# ── .po 읽기·쓰기 ──────────────────────────────────────────────
# 언리얼이 내보내는 .po: 항목마다 주석(#. Key, #. SourceLocation, #:) + msgctxt + msgid + msgstr.

def _po_unescape(s):
    return re.sub(r'\\(.)', lambda m: {"n": "\n", "t": "\t", "r": "\r"}.get(m.group(1), m.group(1)), s)


def _po_escape(s):
    return s.replace("\\", "\\\\").replace('"', '\\"').replace("\n", "\\n").replace("\t", "\\t").replace("\r", "\\r")


class PoEntry:
    def __init__(self):
        self.comments = []   # 원래 주석 줄 그대로
        self.ctxt = None
        self.msgid = ""
        self.msgstr = ""

    @property
    def location(self):
        for line in self.comments:
            if line.startswith("#. SourceLocation:"):
                return line.split(":", 1)[1].strip()
        return ""


def read_po(path):
    """(머리말 줄 목록, 항목 목록)을 돌려준다. 머리말 = 첫 msgid "" 항목."""
    with open(path, encoding="utf-8-sig") as f:
        lines = f.read().split("\n")
    entries, cur, field = [], PoEntry(), None
    for raw in lines + [""]:
        line = raw.rstrip("\r")
        if not line.strip():
            if cur.ctxt is not None or cur.msgid or cur.comments:
                entries.append(cur)
            cur, field = PoEntry(), None
            continue
        if line.startswith("#"):
            cur.comments.append(line)
            continue
        m = re.match(r'(msgctxt|msgid|msgstr)\s+"(.*)"$', line)
        if m:
            field = m.group(1)
            value = _po_unescape(m.group(2))
            if field == "msgctxt":
                cur.ctxt = value
            elif field == "msgid":
                cur.msgid = value
            else:
                cur.msgstr = value
            continue
        m = re.match(r'"(.*)"$', line)   # 여러 줄로 이어진 문자열
        if m and field:
            value = _po_unescape(m.group(1))
            if field == "msgctxt":
                cur.ctxt += value
            elif field == "msgid":
                cur.msgid += value
            else:
                cur.msgstr += value
    header = entries.pop(0) if entries and entries[0].ctxt is None and entries[0].msgid == "" else None
    return header, entries


def write_po(path, header, entries):
    out = []
    if header:
        out += header.comments
        out.append('msgid ""')
        out.append('msgstr ""')
        for part in header.msgstr.split("\n"):
            if part:
                out.append(f'"{_po_escape(part + chr(10))}"')
        out.append("")
    for e in entries:
        out += e.comments
        out.append(f'msgctxt "{_po_escape(e.ctxt or "")}"')
        out.append(f'msgid "{_po_escape(e.msgid)}"')
        out.append(f'msgstr "{_po_escape(e.msgstr)}"')
        out.append("")
    with open(path, "w", encoding="utf-8-sig", newline="\n") as f:
        f.write("\n".join(out))


def po_path(culture):
    return os.path.join(LOC_DIR, culture, "Game.po")


# ── xlsx 읽기 ──────────────────────────────────────────────────

NS = {"m": "http://schemas.openxmlformats.org/spreadsheetml/2006/main",
      "r": "http://schemas.openxmlformats.org/officeDocument/2006/relationships",
      "pr": "http://schemas.openxmlformats.org/package/2006/relationships"}


def _col_index(ref):
    letters = re.match(r"[A-Z]+", ref).group(0)
    n = 0
    for ch in letters:
        n = n * 26 + ord(ch) - 64
    return n - 1


def _text_of(node):
    """<si>나 <is> 안의 글자를 모두 이어 붙인다(서식 있는 글자 조각 포함)."""
    return "".join(t.text or "" for t in node.iter(f"{{{NS['m']}}}t"))


def read_xlsx(path):
    """{탭 이름: [[칸 값, ...], ...]}. 값은 모두 문자열."""
    with zipfile.ZipFile(path) as z:
        shared = []
        if "xl/sharedStrings.xml" in z.namelist():
            root = ET.fromstring(z.read("xl/sharedStrings.xml"))
            shared = [_text_of(si) for si in root.findall("m:si", NS)]
        rels = ET.fromstring(z.read("xl/_rels/workbook.xml.rels"))
        targets = {r.get("Id"): r.get("Target") for r in rels.findall("pr:Relationship", NS)}
        wb = ET.fromstring(z.read("xl/workbook.xml"))
        result = {}
        for sheet in wb.find("m:sheets", NS):
            target = targets[sheet.get(f"{{{NS['r']}}}id")].lstrip("/")
            target = target if target.startswith("xl/") else "xl/" + target
            root = ET.fromstring(z.read(target))
            rows = []
            for row in root.iter(f"{{{NS['m']}}}row"):
                values = {}
                for c in row.findall("m:c", NS):
                    kind, v = c.get("t"), c.find("m:v", NS)
                    if kind == "s":
                        text = shared[int(v.text)] if v is not None else ""
                    elif kind == "inlineStr":
                        node = c.find("m:is", NS)
                        text = _text_of(node) if node is not None else ""
                    else:
                        text = v.text if v is not None and v.text else ""
                        if re.fullmatch(r"-?\d+\.0", text):   # 엑셀이 숫자로 바꾼 '20' → '20.0'
                            text = text[:-2]
                    values[_col_index(c.get("r"))] = text
                width = max(values) + 1 if values else 0
                row_index = int(row.get("r")) - 1
                while len(rows) < row_index:
                    rows.append([])
                rows.append([values.get(i, "") for i in range(width)])
            result[sheet.get("name")] = rows
        return result


# ── xlsx 쓰기 ──────────────────────────────────────────────────
# 상태 색은 '조건부 서식'으로 넣어서, 시트에서 상태를 바꾸면 색도 바로 바뀐다.

STATUS_COLORS = {ST_EMPTY: "F4CCCC", ST_DONE: "FFF2CC", ST_REVIEWED: "D9EAD3", ST_INGAME: "B6D7A8", ST_CHANGED: "F9CB9C"}

_STYLES = """<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<styleSheet xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main">
<fonts count="2"><font><sz val="10"/><name val="맑은 고딕"/></font><font><b/><sz val="10"/><color rgb="FFFFFFFF"/><name val="맑은 고딕"/></font></fonts>
<fills count="4"><fill><patternFill patternType="none"/></fill><fill><patternFill patternType="gray125"/></fill>
<fill><patternFill patternType="solid"><fgColor rgb="FF3C4A5C"/></patternFill></fill>
<fill><patternFill patternType="solid"><fgColor rgb="FFF3F3F3"/></patternFill></fill></fills>
<borders count="1"><border/></borders>
<cellStyleXfs count="1"><xf/></cellStyleXfs>
<cellXfs count="4">
<xf fontId="0" fillId="0" applyAlignment="1"><alignment vertical="top" wrapText="1"/></xf>
<xf fontId="1" fillId="2" applyFont="1" applyFill="1" applyAlignment="1"><alignment vertical="center" wrapText="1"/></xf>
<xf fontId="0" fillId="3" applyFill="1" applyAlignment="1"><alignment vertical="top" wrapText="1"/></xf>
<xf fontId="0" fillId="0" applyAlignment="1"><alignment vertical="top" wrapText="0"/></xf>
</cellXfs>
<dxfs count="%(n)d">%(dxfs)s</dxfs>
</styleSheet>"""
# 칸 서식 번호: 0 = 기본(줄바꿈), 1 = 머리줄, 2 = 원문(회색, 고치지 말 칸), 3 = 줄바꿈 없음(ID·위치)


def _col_letter(i):
    s = ""
    i += 1
    while i:
        i, r = divmod(i - 1, 26)
        s = chr(65 + r) + s
    return s


def _cell(ref, value, style=0):
    value = "" if value is None else str(value)
    if value == "":
        return f'<c r="{ref}" s="{style}"/>'
    if re.fullmatch(r"\d{1,9}", value):
        return f'<c r="{ref}" s="{style}"><v>{value}</v></c>'
    # 엑셀이 xml에 넣을 수 없는 제어 문자 제거
    value = re.sub(r"[\x00-\x08\x0b\x0c\x0e-\x1f]", "", value)
    return f'<c r="{ref}" t="inlineStr" s="{style}"><is><t xml:space="preserve">{escape(value)}</t></is></c>'


class Sheet:
    def __init__(self, name, rows, widths=None, styles=None, status_cols=(), freeze=(1, 0), filter=True):
        self.name, self.rows = name, rows
        self.widths = widths or []
        self.styles = styles or {}          # 칸 번호 → 서식 번호(머리줄 제외)
        self.status_cols = status_cols      # 상태 드롭다운·색을 넣을 칸 번호
        self.freeze, self.filter = freeze, filter

    def xml(self):
        out = ['<?xml version="1.0" encoding="UTF-8" standalone="yes"?>',
               '<worksheet xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main">']
        frow, fcol = self.freeze
        if frow or fcol:
            ref = f"{_col_letter(fcol)}{frow + 1}"
            pane = "bottomRight" if frow and fcol else ("bottomLeft" if frow else "topRight")
            out.append(f'<sheetViews><sheetView workbookViewId="0"><pane xSplit="{fcol}" ySplit="{frow}" '
                       f'topLeftCell="{ref}" activePane="{pane}" state="frozen"/></sheetView></sheetViews>')
        if self.widths:
            out.append("<cols>" + "".join(f'<col min="{i + 1}" max="{i + 1}" width="{w}" customWidth="1"/>'
                                          for i, w in enumerate(self.widths)) + "</cols>")
        out.append("<sheetData>")
        for r, row in enumerate(self.rows):
            cells = "".join(_cell(f"{_col_letter(c)}{r + 1}", v, 1 if r == 0 else self.styles.get(c, 0))
                            for c, v in enumerate(row))
            out.append(f'<row r="{r + 1}">{cells}</row>')
        out.append("</sheetData>")
        width = max((len(r) for r in self.rows), default=1)
        last = max(len(self.rows), 2)
        if self.filter and self.rows:
            out.append(f'<autoFilter ref="A1:{_col_letter(width - 1)}{last}"/>')
        for c in self.status_cols:
            rng = f"{_col_letter(c)}2:{_col_letter(c)}{last + 500}"
            for i, status in enumerate(STATUSES):
                out.append(f'<conditionalFormatting sqref="{rng}"><cfRule type="cellIs" dxfId="{i}" priority="{i + 1}" '
                           f'operator="equal"><formula>"{escape(status)}"</formula></cfRule></conditionalFormatting>')
        if self.status_cols:
            sqref = " ".join(f"{_col_letter(c)}2:{_col_letter(c)}{last + 500}" for c in self.status_cols)
            out.append(f'<dataValidations count="1"><dataValidation type="list" allowBlank="1" sqref="{sqref}">'
                       f'<formula1>"{escape(",".join(STATUSES))}"</formula1></dataValidation></dataValidations>')
        out.append("</worksheet>")
        return "\n".join(out)


def write_xlsx(path, sheets):
    dxfs = "".join(f'<dxf><fill><patternFill><bgColor rgb="FF{STATUS_COLORS[s]}"/></patternFill></fill></dxf>' for s in STATUSES)
    n = len(sheets)
    files = {
        "[Content_Types].xml":
            '<?xml version="1.0" encoding="UTF-8" standalone="yes"?>'
            '<Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types">'
            '<Default Extension="rels" ContentType="application/vnd.openxmlformats-package.relationships+xml"/>'
            '<Default Extension="xml" ContentType="application/xml"/>'
            '<Override PartName="/xl/workbook.xml" ContentType="application/vnd.openxmlformats-officedocument.spreadsheetml.sheet.main+xml"/>'
            '<Override PartName="/xl/styles.xml" ContentType="application/vnd.openxmlformats-officedocument.spreadsheetml.styles+xml"/>'
            + "".join(f'<Override PartName="/xl/worksheets/sheet{i + 1}.xml" '
                      f'ContentType="application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml"/>' for i in range(n))
            + "</Types>",
        "_rels/.rels":
            '<?xml version="1.0" encoding="UTF-8" standalone="yes"?>'
            '<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">'
            '<Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument" Target="xl/workbook.xml"/>'
            "</Relationships>",
        "xl/workbook.xml":
            '<?xml version="1.0" encoding="UTF-8" standalone="yes"?>'
            '<workbook xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main" '
            'xmlns:r="http://schemas.openxmlformats.org/officeDocument/2006/relationships"><sheets>'
            + "".join(f'<sheet name="{escape(s.name)}" sheetId="{i + 1}" r:id="rId{i + 1}"/>' for i, s in enumerate(sheets))
            + "</sheets></workbook>",
        "xl/_rels/workbook.xml.rels":
            '<?xml version="1.0" encoding="UTF-8" standalone="yes"?>'
            '<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">'
            + "".join(f'<Relationship Id="rId{i + 1}" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet" '
                      f'Target="worksheets/sheet{i + 1}.xml"/>' for i in range(n))
            + f'<Relationship Id="rId{n + 1}" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/styles" Target="styles.xml"/>'
            "</Relationships>",
        "xl/styles.xml": _STYLES % {"n": len(STATUSES), "dxfs": dxfs},
    }
    for i, s in enumerate(sheets):
        files[f"xl/worksheets/sheet{i + 1}.xml"] = s.xml()
    tmp = path + ".tmp"
    with zipfile.ZipFile(tmp, "w", zipfile.ZIP_DEFLATED) as z:
        for name, data in files.items():
            z.writestr(name, data)
    os.replace(tmp, path)   # 엑셀에서 열려 있으면 여기서 실패한다 → 닫고 다시 실행


# ── 시트 내용 ↔ 표 ─────────────────────────────────────────────

class Row:
    def __init__(self, id_):
        self.id = id_
        self.location = ""
        self.source = ""
        self.context = ""
        self.max_len = ""
        self.trans = {c: "" for c in CULTURES}
        self.status = {c: ST_EMPTY for c in CULTURES}
        self.reviewer = ""
        self.memo = ""
        self.problems = ""


def table_to_rows(table):
    """'번역' 탭 표 → Row 목록. 칸은 머리줄 이름으로 찾아서, 사람이 칸 순서를 바꿔도 읽힌다."""
    if not table:
        return []
    head = {name: i for i, name in enumerate(table[0])}

    def get(row, name):
        i = head.get(name)
        return row[i] if i is not None and i < len(row) else ""

    rows = []
    for raw in table[1:]:
        id_ = get(raw, "ID")
        if not id_:
            continue
        r = Row(id_)
        r.location, r.source, r.context = get(raw, "위치"), get(raw, "원문(ko)"), get(raw, "맥락")
        r.max_len, r.reviewer, r.memo, r.problems = get(raw, "최대 길이"), get(raw, "검수자"), get(raw, "메모"), get(raw, "검사")
        for c in CULTURES:
            tcol, scol = lang_cols(c)
            r.trans[c] = get(raw, tcol)
            r.status[c] = get(raw, scol) or (ST_DONE if r.trans[c] else ST_EMPTY)
        rows.append(r)
    return rows


def rows_to_sheet(rows):
    table = [all_columns()]
    for r in rows:
        line = [r.id, r.location, r.source, r.context, r.max_len, str(len(r.source))]
        for c in CULTURES:
            line += [r.trans[c], r.status[c]]
        table.append(line + [r.reviewer, r.memo, r.problems])
    widths = [14, 28, 40, 24, 8, 7] + [40, 11] * len(CULTURES) + [10, 24, 36]
    styles = {0: 3, 1: 3, 2: 2, 5: 2}
    status_cols = [len(FIXED_COLS) + i * 2 + 1 for i in range(len(CULTURES))]
    return Sheet("번역", table, widths, styles, status_cols, freeze=(1, 3))


def default_glossary():
    return [["원문(ko)"] + CULTURES + ["메모"],
            ["스탠스", "", "", "", "", "예: 한손·양손 자세"],
            ["그로기", "", "", "", "", ""]]


def summary_sheet(rows):
    table = [["언어"] + STATUSES + ["전체", "진행률(검수됨 이상)"]]
    for c in CULTURES:
        counts = [sum(1 for r in rows if r.status[c] == s) for s in STATUSES]
        done = counts[STATUSES.index(ST_REVIEWED)] + counts[STATUSES.index(ST_INGAME)]
        rate = f"{done * 100 // max(len(rows), 1)}%"
        table.append([f"{c} ({CULTURE_NAMES[c]})"] + [str(n) for n in counts] + [str(len(rows)), rate])
    return Sheet("요약", table, [18] + [11] * len(STATUSES) + [8, 18], filter=False, freeze=(1, 0))


def load_sheet():
    if not os.path.exists(SHEET_PATH):
        return [], default_glossary(), []
    book = read_xlsx(SHEET_PATH)
    rows = table_to_rows(book.get("번역", []))
    glossary = book.get("용어집") or default_glossary()
    archive = table_to_rows(book.get("보관", []))
    return rows, glossary, archive


def save_sheet(rows, glossary, archive):
    archive_sheet = rows_to_sheet(archive)
    archive_sheet.name = "보관"
    gloss_width = [16] + [24] * len(CULTURES) + [30]
    write_xlsx(SHEET_PATH, [rows_to_sheet(rows), Sheet("용어집", glossary, gloss_width), summary_sheet(rows), archive_sheet])


# ── 검사 ───────────────────────────────────────────────────────

RE_ARG = re.compile(r"\{[^{}]*\}")                 # {0}, {Damage}, {Count}|plural(...) 등
RE_TAG = re.compile(r"<[A-Za-z][^<>]*>|</>")        # 서식 있는 글자 태그 <b>…</>


def check_row(r, glossary):
    """줄 하나의 문제를 언어별로 모은다. 결과 = ['en: 자리표시자 {0} 빠짐', ...]"""
    problems = []
    src_args, src_tags = sorted(RE_ARG.findall(r.source)), sorted(RE_TAG.findall(r.source))
    limit = int(r.max_len) if r.max_len.strip().isdigit() else None
    terms = [g for g in glossary[1:] if g and g[0]]
    for ci, c in enumerate(CULTURES):
        t = r.trans[c]
        if not t:
            if r.status[c] in (ST_REVIEWED, ST_INGAME):
                problems.append(f"{c}: 번역이 비었는데 상태가 '{r.status[c]}'")
            continue
        if sorted(RE_ARG.findall(t)) != src_args:
            problems.append(f"{c}: 자리표시자 다름 (원문 {' '.join(src_args) or '없음'})")
        if sorted(RE_TAG.findall(t)) != src_tags:
            problems.append(f"{c}: 서식 태그 다름")
        if t.count("\n") != r.source.count("\n"):
            problems.append(f"{c}: 줄바꿈 수 다름 (원문 {r.source.count(chr(10))}개)")
        if t != t.strip() and r.source == r.source.strip():
            problems.append(f"{c}: 앞뒤 공백")
        if limit is not None and len(t) > limit:
            problems.append(f"{c}: {len(t)}자 > 최대 {limit}자")
        for g in terms:
            fixed = g[ci + 1] if ci + 1 < len(g) else ""
            if fixed and g[0] in r.source and fixed.lower() not in t.lower():
                problems.append(f"{c}: 용어집 '{g[0]}' → '{fixed}' 안 씀")
    return problems


def run_checks(rows, glossary):
    total = 0
    for r in rows:
        found = check_row(r, glossary)
        r.problems = "\n".join(found)
        total += len(found)
    return total


# ── 명령 ───────────────────────────────────────────────────────

def cmd_export():
    """.po → 시트. 시트에 있던 번역·상태·메모를 우선 남기고, 원문이 바뀐 줄은 '원문 변경됨'으로 표시."""
    _, native = read_po(po_path(NATIVE))
    po_trans = {}
    for c in CULTURES:
        _, entries = read_po(po_path(c))
        po_trans[c] = {e.ctxt: e.msgstr for e in entries}

    old_rows, glossary, archive = load_sheet()
    old = {r.id: r for r in old_rows}
    archived = {r.id: r for r in archive}

    rows, changed, added = [], 0, 0
    for e in native:
        prev = old.pop(e.ctxt, None) or archived.pop(e.ctxt, None)
        r = prev or Row(e.ctxt)
        if prev is None:
            added += 1
            for c in CULTURES:
                r.trans[c] = po_trans[c].get(e.ctxt, "")
                r.status[c] = ST_DONE if r.trans[c] else ST_EMPTY
        elif prev.source != e.msgid:
            changed += 1
            for c in CULTURES:
                if r.trans[c]:
                    r.status[c] = ST_CHANGED
        r.source, r.location = e.msgid, e.location
        rows.append(r)

    # 게임에서 사라진 문구 중 번역이 있던 것은 '보관' 탭으로
    gone = [r for r in old.values() if any(r.trans.values())]
    archive = list(archived.values()) + gone
    problems = run_checks(rows, glossary)
    save_sheet(rows, glossary, archive)
    print(f"시트 갱신: 문구 {len(rows)}개 (새 문구 {added}, 원문 변경 {changed}, 보관으로 옮김 {len(gone)}), 검사 문제 {problems}건")
    print(f"  → {SHEET_PATH}")


def cmd_check():
    rows, glossary, archive = load_sheet()
    if not rows:
        sys.exit("시트가 없습니다. 먼저 'loc.bat gather' 를 실행하세요.")
    total = run_checks(rows, glossary)
    for r in rows:
        if r.problems:
            print(f"[{r.id}] {r.source[:40]!r}")
            for p in r.problems.split("\n"):
                print(f"    {p}")
    save_sheet(rows, glossary, archive)
    print(f"검사 문제 {total}건. 시트의 '검사' 칸에도 적었습니다.")
    return total


def cmd_import():
    """시트 → 언어별 .po. msgid는 지금 원문으로 쓴다(언리얼은 msgctxt+msgid가 맞는 번역만 받아들인다)."""
    rows, glossary, _ = load_sheet()
    if not rows:
        sys.exit("시트가 없습니다.")
    by_id = {r.id: r for r in rows}
    for c in CULTURES:
        header, entries = read_po(po_path(c))
        filled = 0
        for e in entries:
            r = by_id.get(e.ctxt)
            # 원문이 바뀐 줄도 예전 번역을 넣는다(한국어가 그대로 나오는 것보다 낫다). 상태로 다시 번역할 줄을 관리.
            e.msgstr = r.trans[c] if r and r.source == e.msgid else e.msgstr
            filled += bool(e.msgstr)
        write_po(po_path(c), header, entries)
        print(f"{c}: {filled}/{len(entries)}개 번역을 .po에 씀")
    problems = run_checks(rows, glossary)
    if problems:
        print(f"주의: 검사 문제 {problems}건이 있습니다. 'loc.bat check' 로 확인하세요.")


def run_unreal(*configs):
    """에디터 명령줄(GatherText)로 언리얼 현지화 단계를 돌린다. 엔진 Python으로 실행했을 때 엔진 위치를 안다."""
    engine = os.environ.get("UE_ROOT") or os.path.normpath(os.path.join(os.path.dirname(sys.executable), *[".."] * 5))
    editor = os.path.join(engine, "Engine", "Binaries", "Win64", "UnrealEditor-Cmd.exe")
    if not os.path.exists(editor):
        sys.exit(f"UnrealEditor-Cmd.exe 를 찾지 못했습니다: {editor}\n  UE_ROOT 환경 변수에 엔진 폴더(UE_5.8)를 넣어 주세요.")
    log = os.path.join(PROJECT_DIR, "Saved", "Logs", "Localization.log")
    config = ";".join(f"Config/Localization/Game_{c}.ini" for c in configs)
    print(f"언리얼 실행: {', '.join(configs)} (1~2분 걸림, 로그: {log})")
    code = subprocess.call([editor, UPROJECT, "-run=GatherText", f"-config={config}",
                            "-unattended", "-nopause", "-nosplash", "-NoShaderCompile", f"-abslog={log}"],
                           stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    with open(log, encoding="utf-8", errors="replace") as f:
        warnings = [l.strip() for l in f if "LogLocTextHelper: Warning" in l or ": Error:" in l]
    for w in warnings[:20]:
        print("  " + w.split("]", 2)[-1])
    if code != 0:
        sys.exit(f"언리얼 명령 실패(코드 {code}). 로그를 확인하세요.")


def main():
    parser = argparse.ArgumentParser(description="Knight 번역 시트 도구")
    parser.add_argument("command", choices=["gather", "export", "check", "import"])
    parser.add_argument("--no-unreal", action="store_true", help="언리얼 명령은 건너뜀")
    args = parser.parse_args()
    try:
        if args.command == "gather":
            if not args.no_unreal:
                run_unreal("Gather", "Export")
            cmd_export()
        elif args.command == "export":
            cmd_export()
        elif args.command == "check":
            cmd_check()
        elif args.command == "import":
            cmd_import()
            if not args.no_unreal:
                run_unreal("Import", "Compile")
                print("완료. 에디터를 다시 열거나 'Preview Game Language'로 확인하세요.")
    except PermissionError:
        sys.exit(f"파일을 쓸 수 없습니다. 엑셀에서 {os.path.basename(SHEET_PATH)} 를 닫고 다시 실행하세요.")


if __name__ == "__main__":
    main()
