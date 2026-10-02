# Knight

Knight는 NBcamp Unreal Track 9기 Team 3의 Unreal Engine 5 C++ 프로젝트입니다. 팀에서 사용할 정확한 엔진 버전은 **미정**입니다. 현재 `knight.uproject`에는 `EngineAssociation: 5.8`이 기록되어 있습니다.

팀 저장소: [NBcampUnrealTrack/9th-Team3-Final-Project](https://github.com/NBcampUnrealTrack/9th-Team3-Final-Project)

## 저장소 받기

Git과 Git LFS를 설치한 뒤 다음 명령을 실행합니다.

```sh
git lfs install
git clone https://github.com/NBcampUnrealTrack/9th-Team3-Final-Project.git knight
cd knight
git lfs pull
```

이미 클론한 저장소에서도 `git lfs pull`로 LFS 파일을 내려받을 수 있습니다.

## 프로젝트 열기

저장소 루트의 `knight.uproject` 파일을 Unreal Editor로 엽니다. IDE 프로젝트 파일이 필요하면 Windows 탐색기에서 `knight.uproject` 파일을 우클릭하고 **Generate Visual Studio project files**를 선택해 다시 생성합니다.

Git에는 `.uproject`, `Config/`, `Content/`, `Source/`, 필요한 `Build/` 파일, 그리고 플러그인의 `Source/`, `Content/`, `Config/` 등을 포함합니다. `Binaries/`, `DerivedDataCache/`, `Intermediate/`, `Saved/` 및 생성된 IDE 프로젝트 파일은 포함하지 않습니다. 이 생성 폴더 제외 규칙은 플러그인 내부에도 적용됩니다.

대용량 에셋을 추가하기 전에 해당 확장자가 `.gitattributes`에서 Git LFS 대상으로 설정되어 있는지 확인하세요.
