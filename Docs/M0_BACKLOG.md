# M0 — Unreal Project Foundation

## Outcome

В редакторе запускается `Test Match` с listen server и двумя игроками. У каждого есть свободная камера и свой герой; RMB-команда проходит через server RPC и двигает только принадлежащего игроку героя. GAS корректно инициализирован на PlayerState/hero avatar.

## Задачи

### Foundation

- [x] Создать `.uproject`, Editor/Game targets и gameplay module.
- [x] Подключить GameplayAbilities, GameplayTags, GameplayTasks, NavigationSystem и EnhancedInput.
- [x] Добавить базовые config и gameplay tags.

### Match core

- [x] Реализовать `AEFGameMode : AGameMode`.
- [x] Реализовать `AEFGameState : AGameState`.
- [x] Реализовать `AEFPlayerState` с progression вне GAS.
- [x] Развести lore faction и match side типами enum.

### GAS / combat

- [x] Добавить `UEFAbilitySystemComponent` subclass.
- [x] Добавить `UEFHeroAttributeSet` с combat-only attributes.
- [x] Инициализировать Owner/Avatar через PlayerState/hero.
- [x] Добавить каркас `UEFCombatComponent` и Gameplay Event для basic attack contact.

### Controls/network

- [x] Добавить свободную `AEFMOBACameraPawn`.
- [x] Добавить owner-only `ControlledHero` в PlayerController.
- [x] Реализовать server-authoritative click-to-move order.
- [x] Создать `L_TestMatch` в Editor и назначить стартовой картой.
- [x] Добавить C++ fallback-позиции для двух игроков и временное серверное движение без NavMesh.
- [ ] Добавить `NavMeshBoundsVolume` и проверить полноценный pathfinding.
- [ ] Проверить listen server + 2 clients.

### Verification

- [x] UnrealBuildTool сгенерировал необходимые build files для UE 5.8.
- [x] Собрать `EclipseFrontEditor Development` без warnings/errors.
- [x] Загрузить `L_TestMatch` без окна и пройти Map Check: 0 ошибок, 0 предупреждений.
- [ ] Пройти все сценарии `M0_TEST_PLAN.md`.

## Definition of Done

- Сборка Editor target успешна.
- Два клиента входят в один PIE match и получают разных героев.
- Каждый игрок управляет только своим героем.
- Сервер отклоняет NaN/Infinity и чрезмерно далёкую destination.
- Камера каждого клиента свободна и возвращается к своему герою.
- `showdebug abilitysystem` отображает ASC с правильным avatar.
- Повторное подключение/respawn не оставляет ASC привязанным к старому avatar.

## Следующий milestone

M1: одна линия, команды attack/move, базовая атака с уроном, death/respawn, один creep type и минимальный health UI.
