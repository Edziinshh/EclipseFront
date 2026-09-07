# ECLIPSE FRONT — Technical Design / Architecture Notes

Версия 0.2, 2026-09-04.

## Цели архитектуры

- Сетевой фундамент с первого дня.
- Один источник истины для каждого типа состояния.
- C++ framework, расширяемый Blueprint/Data Assets.
- Отделение orders, movement, combat, abilities и presentation.
- Возможность перейти от 1v1 к 5v5 без переписывания базовой модели владения.

## Match framework

`AEFGameMode : AGameMode` существует только на сервере, управляет lifecycle, входом игрока, spawn/respawn и победой. `AGameMode` выбран из-за встроенной модели match states.

`AEFGameState : AGameState` реплицирует phase, elapsed match time и в будущем состояния узлов/целей. Клиент никогда не использует GameMode как источник UI-состояния.

Предлагаемые фазы:

```text
WaitingForPlayers -> HeroSelection -> PreGame -> InProgress -> PostGame
```

M0 начинает матч при двух игроках и использует встроенный `MatchState` плюс компактную реплицируемую фазу.

## Владение игроком и камерой

PlayerController владеет `AEFMOBACameraPawn`, а не непосредственно героем. В контроллере есть owner-only ссылка `ControlledHero`. Это заранее поддерживает selection, summons и multi-unit control.

Поскольку PlayerState может иметь только один стандартный `Pawn` back-reference, hero не переиспользует `APawn::PlayerState`: его связь с владельцем реплицируется отдельным `OwningPlayerState`. Так camera pawn остаётся корректно possessed, а AI-controlled hero получает тот же PlayerState как GAS owner без повреждения встроенных ссылок Unreal.

```text
Local input
  -> AEFPlayerController
  -> ServerRequestMove(destination)
  -> ownership + finite/range/nav validation
  -> navigation order for ControlledHero
```

Камера локально перемещается независимо от сетевого героя. Её положение не является gameplay authority.

Для M0 существует development-only fallback: если на карте меньше двух `PlayerStart`, `AEFGameMode` рассчитывает две симметричные позиции и привязывает их к поверхности трассировкой. Если NavMesh ещё не настроен, сервер проверяет destination по collision поверхности и двигает героя напрямую через CharacterMovement. Этот режим нужен только для первого smoke test; карта с препятствиями обязана использовать NavigationSystem.

На `L_TestMatch` fallback прямого движения отключён по умолчанию после добавления `NavMeshBoundsVolume`. Обычный клик RMB отправляет надёжную команду, а удержание RMB локально семплирует положение курсора с интервалом 0,1 секунды. Новое промежуточное назначение отправляется только после смещения курсора минимум на 75 units через unreliable RPC: устаревшие точки маршрута не должны накапливаться в надёжной сетевой очереди. Сервер повторно выполняет все проверки destination и дополнительно ограничивает частоту таких обновлений.

Attack-move задаётся верхней клавишей `4` и последующим LMB по земле. Клиент передаёт только destination; сервер валидирует её тем же NavigationSystem-путём, хранит активный приказ и с интервалом ищет ближайшую допустимую вражескую цель в acquisition radius. Герои, крипы и только уязвимые строения являются допустимыми целями. После смерти или потери текущей цели герой продолжает движение к исходной точке. RMB, прямая атака, Stop и Hold Position отменяют attack-move. Выбор цели и возобновление пути не доверяются клиенту.

Presentation текущего приказа строится из owner-only реплицируемых полей `AEFPlayerController`: тип приказа, destination и server-selected target. Клиент рисует только локальную индикацию и не влияет ею на движение или combat. Сервер очищает presentation state при завершении/отмене приказа, смерти героя и PostGame. Линия обозначает направление к destination, а не рассчитанный NavigationSystem path.

## GAS

`UEFAbilitySystemComponent` наследует `UAbilitySystemComponent`. Параллельная ability-система не создаётся.

ASC принадлежит `AEFPlayerState`, поэтому ability state переживает смерть/замену avatar. При назначении героя выполняется:

```text
OwnerActor  = AEFPlayerState
AvatarActor = AEFHeroCharacter
```

`UEFHeroAttributeSet` хранит только боевые параметры, которые естественно изменяются Gameplay Effects:

- Health / MaxHealth
- Mana / MaxMana
- AttackDamage / AttackSpeed / AttackRange
- Armor / MagicResistance
- MoveSpeed

Level, Experience, Gold и KDA реплицируются в `AEFPlayerState` и имеют одного владельца данных вне AttributeSet.

## Basic attack: hybrid Combat + GAS

```text
Player order
  -> PlayerController / future OrderComponent
  -> UEFCombatComponent attack state machine
  -> wind-up / target validation / projectile or melee contact
  -> Gameplay Event
  -> Gameplay Ability / Gameplay Effect
  -> damage and procs
```

CombatComponent отвечает за подход в радиус, attack point, backswing, текущую цель и отмену. GAS отвечает за modifiers, crit/bash/lifesteal/status и финальные effects. В M1 контакт отправляет gameplay event и instant effect с `Data.Damage.BasicAttack`; `AttributeSet` потребляет server-only meta-attribute Damage, применяет базовую формулу armor и уменьшает Health. Это прототипная формула, а не финальный баланс.

## Server authority

Клиент передаёт только намерение. Сервер проверяет:

- принадлежит ли команда этому controller;
- существует ли controlled hero и способен ли он принимать order;
- координаты конечны;
- destination находится в разумном радиусе мира;
- navigation system может построить путь или принять цель;
- attack target существует, враждебен, relevant и допустим по дистанции/состоянию.

Damage, XP, Gold и map state никогда не принимаются от клиента готовыми числами.

## Living Battlefield

Карта содержит полный набор потенциальных маршрутов. Node state включает/выключает заранее размещённые:

- visual gates/bridges;
- collision blockers;
- smart/nav links или modifiers;
- vision providers;
- teleport endpoints;
- spawn-rule tags.

Сервер реплицирует enum/state и timestamp перехода. Клиенты воспроизводят presentation. Масштабная генерация geometry и полный runtime NavMesh rebuild не являются частью базового решения.

Полная карта: `2 mirrored node pairs + 1 central node`. Прототип доказывает систему на одном central node, не нарушая будущую схему.

## First Lane и волны крипов

В M2 match flow создаёт один server-only `AEFWaveSpawner`. Он не реплицирует собственное состояние: сервер создаёт реплицируемых `AEFCreepCharacter`, а клиенты получают их сторону и движение обычной actor replication.

Маршрут линии — короткий упорядоченный набор заранее определённых точек. Контрольные точки размещаются только на заведомо доступных участках и не проецируются внутрь blocker; AI двигается между ними через существующий NavMesh, который сам прокладывает обход. Препятствия меняют найденный путь, но волна не инициирует runtime rebuild мира. Одинаковые параметры и одновременный server spawn обеспечивают симметрию Dawn/Dusk. По достижении последней точки крип уничтожается сервером; lifespan служит только аварийной очисткой.

Срез M2.1 намеренно не смешивает route validation с aggro/combat. Крип временно игнорирует канал столкновений Pawn, поэтому встречные волны не создают физический deadlock. Следующий срез добавит явную state machine `Marching -> Engaging -> Attacking -> Returning`, а damage будет подключён к существующему пути `CombatComponent + GAS`.

В M2.2 player-owned GAS по-прежнему принадлежит `AEFPlayerState`, а `AEFHeroCharacter` остаётся avatar. Серверные NPC не получают фиктивные PlayerState: `AEFCreepCharacter` является owner/avatar собственного `UEFAbilitySystemComponent` в режиме Minimal replication и использует тот же боевой AttributeSet/Gameplay Effect. `UEFCombatComponent` работает с героями и крипами как с допустимыми combat units, поэтому отдельного пути расчёта урона нет.

Коллизия Pawn у крипов остаётся query-independent от остановки боя: агро и attack range определяет серверная combat state machine. Это исключает физические deadlock волн. Полноценный body blocking откладывается до отдельного среза с separation/RVO и проверкой предсказуемости на сервере.

Крип периодически пересматривает окружение даже во время текущей атаки. Вражеские lane creeps имеют более высокий aggro priority, чем герой или строение: если герой увёл волну и рядом появилась встречная волна, сервер меняет цель на ближайшего вражеского крипа. Это prototype policy M3.1; tower aggro и полноценная threat table будут развиваться отдельно.

## Lane towers

Расширение цепочки зданий, реплицируемая защита, усиление волн и завершение матча: см. M3_SIEGE.md. Однолинейный прототип содержит 16 строений.

В M3.1 `AEFLaneTower` — неподвижный server-authoritative combat unit со своим `UEFAbilitySystemComponent` в режиме Minimal replication и тем же боевым AttributeSet. Герои, крипы и башни используют один `UEFCombatComponent` и один Gameplay Effect расчёта basic-attack damage. Отдельной формулы урона для строений нет.

На старте Test Match `AEFGameMode` создаёт зеркальную пару башен Dawn/Dusk на заранее определённых позициях линии. Башня выбирает ближайшего вражеского крипа и только при отсутствии крипов — героя. Башня не преследует вышедшую из радиуса цель. Health, сторона, attack-contact presentation и смерть реплицируются с сервера; первая версия использует мгновенный contact, а визуальный projectile отложен до M3.2.

В M3.2 башня создаёт реплицируемый `AEFTowerProjectile` после завершения wind-up. Снаряд визуально следует за зафиксированной целью, но только сервер определяет достижение и вызывает единый `UEFCombatComponent::ResolveBasicAttackImpact`; клиент не сообщает hit или damage. Если герой наносит basic-attack damage вражескому герою в радиусе союзной тому башни, combat flow уведомляет башню, и та временно получает forced hero target. В обычном состоянии сохраняется приоритет lane creeps.

Отображение attack range по удержанию `Alt` является локальной debug/gameplay presentation в PlayerController. Радиусы читаются из реплицированных боевых attributes героев, крипов и башен; круги не реплицируются и не участвуют в серверных проверках дальности.

## Last hit и progression

При смерти крипа сервер получает original instigator из Gameplay Effect context. Gold и счётчик Last Hits получает только `AEFPlayerState`, связанный с героем, нанёсшим последний удар. Experience выдаётся каждому уникальному живому союзному герою в пределах XP radius независимо от того, кто нанёс последний удар. Крип, убитый другим крипом, не выдаёт Gold, но выдаёт XP подходящему герою рядом.

Level, текущий XP внутри уровня, Gold и Last Hits реплицируются в `AEFPlayerState`; они не являются GAS attributes. Временный `AEFHUD` только читает реплицированное состояние локального PlayerState. Порог следующего уровня в прототипе равен `CurrentLevel * 100 XP`, максимальный уровень — 30. Bounty M2: `45 Gold`, `60 XP`, радиус опыта `1200` units. Эти числа являются prototype defaults и позже переходят в Data Assets.

## Respawn

Единственный authoritative flow принадлежит `AEFGameMode`:

```text
Hero death -> GameMode schedules respawn -> spawn/restore avatar
-> PlayerController ControlledHero update -> ASC actor info refresh
```

Компоненты могут сообщать о смерти, но не создают новый pawn самостоятельно.

## Fog of War

Путь внедрения:

1. визуальный FoW и серверная visibility policy;
2. connection-specific relevancy для скрытых actors;
3. hardened replication и аудит утечек данных.

Чёрная маска не считается защитой от читов. Полный FoW не входит в M0.

## Классы M0

| Класс | Ответственность |
|---|---|
| `AEFGameMode` | lifecycle, hero spawn, respawn owner |
| `AEFGameState` | реплицируемая фаза и время |
| `AEFPlayerState` | ASC, attributes, level/XP/gold/KDA |
| `AEFPlayerController` | input/orders, RPC validation, controlled hero |
| `AEFMOBACameraPawn` | локальная свободная камера и recenter |
| `AEFHeroCharacter` | avatar, movement, ASC binding |
| `UEFAbilitySystemComponent` | проектное расширение GAS |
| `UEFHeroAttributeSet` | боевые attributes |
| `UEFCombatComponent` | auto-attack state skeleton + GAS events |

## Решения, отложенные до данных/прототипа

- Iris/Replication Graph strategy и dedicated-server sizing.
- Fixed-step unit simulation для больших волн.
- Prediction policy отдельных abilities.
- Финальная armor/magic-resistance formula.
- Draft, reconnect, backend matchmaking и persistence.
