# ECLIPSE FRONT

Competitive MOBA на Unreal Engine 5 с серверной авторитетностью и полем боя, которое меняет доступные маршруты во время матча.

Текущий этап: **M3 — Lane Structures**. Это пока не «5v5-игра», а сетевой `Test Match`: проверенный combat loop, волны крипов и однолинейная цепочка осады `T1 → T2 → T3 → 2×T4 → Nexus` с двумя бараками на сторону.

## Быстрый старт

1. Установить Unreal Engine 5.8 с компонентами C++ и Visual Studio 2022.
2. Открыть `EclipseFront.uproject` и согласиться с генерацией project files/сборкой модулей.
3. Открыть готовую карту `/Game/Maps/L_TestMatch`; `AEFGameMode` назначен глобально через конфигурацию.
4. На `L_TestMatch` уже размещён `NavMeshBoundsVolume`, покрывающий линию и обе базы. Старые препятствия M0 удалены; прямое движение без NavMesh отключено.
5. В PIE выбрать `Number of Players = 2` и `Net Mode = Play As Listen Server`.

Проверки: [Docs/M0_TEST_PLAN.md](Docs/M0_TEST_PLAN.md), [Docs/M1_TEST_PLAN.md](Docs/M1_TEST_PLAN.md), [Docs/M2_TEST_PLAN.md](Docs/M2_TEST_PLAN.md) и [Docs/M3_TEST_PLAN.md](Docs/M3_TEST_PLAN.md).

## Структура

- `Docs/GDD_v0.2.md` — игровой дизайн и границы прототипа.
- `Docs/TECHNICAL_ARCHITECTURE.md` — архитектура и сетевые решения.
- `Docs/DEVLOG.md` — хронология изменений, проверок и следующих шагов.
- `Docs/M0_BACKLOG.md` — milestone, критерии готовности и задачи.
- `Docs/M1_BACKLOG.md` — текущий combat milestone и следующие задачи.
- `Docs/M2_BACKLOG.md` — первый lane milestone и порядок развития крипов.
- `Docs/M3_BACKLOG.md` — башни, objectives и следующие шаги линии.
- `Docs/DEBUGGING.md` — категории логов и порядок отладки сетевого матча.
- `Source/EclipseFront` — C++ gameplay module.
- `Config` — gameplay tags, maps и сетевые настройки.

## Принципы

- C++ определяет правила, сеть и расширяемый framework.
- Blueprint/Data Assets определяют героев, способности, числа, VFX и контент.
- Клиент отправляет намерение; сервер проверяет и применяет результат.
- Лор-фракция героя не ограничивает сторону, за которую он играет в матче.
- Живая карта переключает заранее подготовленные пути, а не перестраивает мир во время матча.

## Статус M0

- [x] Unreal C++ project/module foundation.
- [x] `AGameMode` / `AGameState` / `APlayerState` / `APlayerController`.
- [x] Кастомный `UEFAbilitySystemComponent` и боевой `AttributeSet`.
- [x] XP, gold и level вне `AttributeSet`.
- [x] Независимая MOBA-камера.
- [x] Server-authoritative click-to-move command path.
- [x] Гибридный `CombatComponent + GAS` каркас basic attack.
- [x] Собрать проект локально с установленным UE 5.8.
- [x] Создать и назначить стартовой карту `L_TestMatch`.
- [x] Добавить C++ fallback-позиции и временное прямое движение для карты без editor setup.
- [x] Добавить `NavMeshBoundsVolume` и препятствия для полноценной проверки pathfinding.
- [x] Пройти PIE-тест на двух игроках.

## Статус M1

- [x] LMB selection и контекстная RMB attack-order.
- [x] Server-authoritative approach / attack point / backswing.
- [x] Basic attack damage через Gameplay Effect и meta damage attribute.
- [x] Реплицируемый Health и временная world-space полоска здоровья.
- [x] Серверные death/respawn и K/D.
- [x] Пройти [ручной сетевой тест M1](Docs/M1_TEST_PLAN.md).

## Статус M2

- [x] Серверный spawner симметричных волн.
- [x] Первый melee creep с реплицируемой стороной матча.
- [x] Движение крипов по заранее заданному маршруту через NavMesh.
- [x] Проверить волны на listen server и клиенте.
- [x] Добавить creep aggro и общий `CombatComponent + GAS` бой.
- [x] Проверить creep combat на listen server и клиенте.
- [x] Добавить серверные Gold/XP/Last Hits и рост уровня.
- [x] Добавить временный HUD progression.
- [x] Проверить M2.3 на listen server и клиенте.

## Статус M3

- [x] Добавить по одной серверной башне Dawn и Dusk.
- [x] Подключить башни к общему `CombatComponent + GAS`.
- [x] Разрешить героям и крипам атаковать вражеские башни.
- [x] Добавить приоритетный re-aggro крипов с героя на подошедшую вражескую волну.
- [x] Собрать и пройти [ручной сетевой тест M3.1](Docs/M3_TEST_PLAN.md).
- [x] Реализовать однолинейную цепочку из 16 строений, защиту внутренних целей и Nexus.
- [x] Добавить melee/ranged barracks и усиление соответствующих новых волн противника.
- [x] Реплицировать усиления, победителя и состояние завершённого матча через GameState.
- [x] Собрать UE 5.8 и пройти Automation + headless listen server с двумя клиентами.
- [ ] Пройти визуальный сценарий M3.3–M3.4 из [M3_SIEGE.md](Docs/M3_SIEGE.md).
