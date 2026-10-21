# 퀘스트 부위 파괴 보상 연결

`AknightRaidGameMode::RecordBrokenPart(FGuid RunId, FName BossId,
AknightMonsterCharacterBase* Monster, FGameplayTag PartTag)`는
`Source/knight/Raid/knightRaidGameMode.h`에 선언되어 있습니다. 파괴가 **확정된 뒤**
게임 스레드에서 호출하세요. 현재 실행의 `RunId`와 퀘스트의 `BossId`, 실제 파괴된
보스 인스턴스, 그 부위의 `Monster.Part.*` 태그를 전달해야 합니다. 실행 중인 대상
보스가 아니거나 이미 기록한 부위, 이전 실행 ID, 다른 보스 ID, 파괴되지 않은
부위라면 `false`를 반환하고 보상을 늘리지 않습니다.

기존 `UknightMonsterPartComponent::BreakPart` 경로는 이미
`OnPartBrokenNative`를 방송하고 GameMode가 자동으로 기록합니다. 이 경로에서는
별도의 `RecordBrokenPart` 호출이 필요하지 않습니다. 다른 파괴 구현이 생겨
기존 이벤트를 쓰지 못할 때만 해당 구현의 확정 지점에 다음처럼 연결하세요.

```cpp
if (UWorld* World = GetWorld()) // 호출자가 Actor/Component인 경우
{
    if (AknightRaidGameMode* RaidMode = World->GetAuthGameMode<AknightRaidGameMode>())
    {
        RaidMode->RecordBrokenPart(RunId, BossId, BrokenMonster, BrokenPartTag);
    }
}
```

`RunId`는 현재 `UknightRaidSubsystem::GetCurrentRunId()`에서, 퀘스트 `BossId`는
`GetActiveQuestRunContext()`에서 얻습니다. 머리 태그는 잔열 퀘스트에서
`잔열의 발화기관`, 꼬리 태그는 `잔열의 꼬리뼈`에 연결되어 있습니다. 다른 보스의
실제 아이템 연결은 각 `DA_Quest_*`의 `Part Break Rewards`를 따릅니다.
성공 결과에 기록된 부위만 지급하며 실패·중단·다음 실행에는 이어지지 않습니다.
