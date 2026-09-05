# ECLIPSE FRONT — правила для Codex

## Язык и коммуникация

- Общение с владельцем проекта — на русском.
- Идентификаторы C++, имена ассетов и commit messages — на английском.
- Любое допущение, влияющее на gameplay или сетевую модель, фиксировать в `Docs/`.

## Технические границы

- Целевая версия: Unreal Engine 5.8, C++17/стандарт, выбранный Unreal Build Tool.
- Gameplay module: `EclipseFront`, API macro: `ECLIPSEFRONT_API`.
- Ядро, репликация, валидация команд и combat rules — C++.
- Настраиваемый контент, presentation и balance data — Blueprint/Data Assets.
- Не добавлять плагины и внешние зависимости без явного обоснования.

## Сеть

- Сервер — единственный источник истины для movement orders, damage, resources, death/respawn и map-node state.
- RPC описывают намерение игрока, проверяют владение, допустимость цели, конечность координат и дистанционные лимиты.
- Не доверять клиентским hit/damage/resource values.
- Новая gameplay-функция считается незавершённой без сценария проверки listen server + два клиента.

## Архитектура

- Match lifecycle: `AEFGameMode : AGameMode`; реплицируемое состояние: `AEFGameState : AGameState`.
- GAS принадлежит `AEFPlayerState`, avatar — текущий `AEFHeroCharacter`.
- Использовать `UEFAbilitySystemComponent : UAbilitySystemComponent`, не параллельную ability-систему.
- В `AttributeSet` держать только боевые параметры, изменяемые эффектами. Level/XP/Gold/KDA — progression/match data в PlayerState.
- Auto-attack state machine живёт в `UEFCombatComponent`; procs, statuses и damage application интегрируются через Gameplay Events/Effects.
- Lore faction и match side — разные типы данных.
- Living Map переключает заранее размещённые gates/blockers/nav links/routes; не запускать масштабный runtime rebuild карты/NavMesh.
- Respawn flow должен иметь одного владельца — `AEFGameMode`; не дублировать его в компонентах и manager-классах.

## Рабочий процесс

- Сначала читать `README.md`, `Docs/GDD_v0.2.md`, `Docs/TECHNICAL_ARCHITECTURE.md` и актуальный milestone.
- Делать маленькие изменения с проверяемым результатом.
- Не редактировать `.uasset`/`.umap` как текст и не создавать фиктивные бинарные ассеты.
- После C++ изменений собирать Editor target, если Unreal установлен; иначе явно отмечать непроверенную сборку.
- Не считать M0 завершённым, пока не пройден `Docs/M0_TEST_PLAN.md`.
