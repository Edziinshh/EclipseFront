# M0 — Test Plan

## Подготовка Editor-контента

1. Открыть `/Game/Maps/L_TestMatch`; она назначена стартовой в `DefaultEngine.ini`.
2. Для первого smoke test можно не добавлять `PlayerStart`: C++ создаёт две fallback-позиции на поверхности. `NavMeshBoundsVolume` также необязателен для первого запуска благодаря development-only direct-move fallback, но обязателен для окончательной проверки pathfinding M0.
3. `AEFGameMode` уже назначен глобально. Blueprint-наследник понадобится позже только для настройки контента.
4. При необходимости создать `BP_EFHeroCharacter` и задать mesh/animation; capsule достаточно для smoke test.
5. Убедиться, что navigation preview покрывает игровую площадь.

## Сценарий A — два игрока

- PIE: 2 players, Play As Listen Server, отдельные окна.
- Ожидание: оба игрока получают camera pawn и отдельного hero actor; match переходит в InProgress при двух подключениях.

## Сценарий B — движение

- На каждом клиенте RMB по разным точкам пола.
- Ожидание: движется только hero этого клиента; результат виден обоим окнам.
- RMB за пределами navigation или очень далеко от героя.
- Ожидание: команда отклоняется или не приводит к неконтролируемому перемещению.

## Сценарий C — камера

- WASD и edge scroll перемещают камеру независимо.
- Удержание `T` возвращает камеру к controlled hero (`Space` переназначен на Stop в M4).
- Движение камеры одного клиента не влияет на второго и gameplay state.

## Сценарий D — GAS

- Выполнить `showdebug abilitysystem` на каждом клиенте.
- Ожидание: Owner — соответствующий PlayerState, Avatar — соответствующий hero; attributes имеют валидные базовые значения.

## Сценарий E — сетевые границы

- Проверить отключение второго клиента и повторный запуск PIE.
- Проверить отсутствие access violation при позднем `OnRep_PlayerState`/`OnRep_ControlledHero`.
- В логе не должно быть повторной выдачи startup abilities/effects (когда они появятся).

## Известные ограничения M0

Карта и Blueprint-ассеты создаются через Unreal Editor и не представлены фиктивными `.umap/.uasset` файлами. Dedicated server, latency simulation, reconnect и production anti-cheat относятся к последующим milestones.
