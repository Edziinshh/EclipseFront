# ECLIPSE FRONT — M1 Combat Loop

Статус: **пройден вручную**. Цель milestone — получить проверяемую 1v1-петлю боя поверх сетевого фундамента M0.

## Реализованный вертикальный срез

- LMB локально выбирает героя и показывает кольцо выбора.
- RMB по земле отменяет атаку и создаёт серверную команду движения.
- RMB по вражескому герою создаёт серверную attack-order.
- `UEFCombatComponent` проверяет сторону матча и состояние целей, подходит в радиус, исполняет attack point и backswing.
- Контакт отправляет `Event.Combat.BasicAttack.Contact` и применяет instant Gameplay Effect с set-by-caller damage.
- `UEFHeroAttributeSet` принимает raw damage через meta-attribute, применяет armor и изменяет Health.
- Полоска здоровья обновляется на сервере и клиентах без обязательных Blueprint-ассетов.
- При нуле Health `AEFGameMode` единолично запускает death/respawn flow; ASC остаётся в PlayerState.
- Kills/Deaths записываются в PlayerState; Health/Mana и `State.Dead` сбрасываются при новом avatar.
- Повторный RMB по текущей цели не сбрасывает attack point/backswing.
- Approach-order к неподвижной цели не перезапускает pathfinding каждый кадр; обновление выполняется только при заметном смещении цели.
- Для маленьких prototype-моделей действует ограниченный context-target assist около точки клика.
- Контакт атаки реплицирует короткий scale-pulse, а подпись стороны показывает текущее Health.
- Атакующий поворачивается к цели, модель делает короткий contact-lunge, а над целью появляется реплицируемое число фактически полученного урона.
- Overhead presentation (сторона, HP, health bar, floating damage) не наследует yaw героя и ориентируется по локальной камере отдельно на каждом клиенте.
- Удержание RMB обновляет move/attack order под курсором с ограниченной частотой; промежуточные move RPC ненадёжные, потому что новое назначение заменяет старое.

## Следующие задачи M1

- [x] Добавить NavMeshBoundsVolume и тестовые препятствия на `L_TestMatch`.
- [x] Проверить в PIE обход препятствий и непрерывное движение при удержании RMB.
- [ ] Заменить временные mesh-индикаторы на WidgetComponent/UMG presentation.
- [x] Добавить combat log и явную prototype-визуализацию contact/damage.
- [x] Добавить первый тип крипа и server-owned wave spawner (перенесено в M2).
- [ ] Добавить aggro, last hit и награду XP/Gold.
- [ ] Добавить автоматизированные functional tests для attack validation и respawn.

## Не входит в текущий срез

Проектильные атаки, crit/evasion, предметы, deny, башни, полноценная анимация, production UI и баланс героев.
