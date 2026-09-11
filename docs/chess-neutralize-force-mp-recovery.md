# 化勁：命中回內

This change supersedes the 化勁 suppression, shield, and cross-status arbitration
rules in the earlier status-design documents.

綿掌 applies one selected 化勁 charge to its main projectile's hit target. The
holder's next successful attack contact restores `40 + 10 × producer star` MP
to the actual hit target, then consumes one charge. Normal hit damage continues.
The original cast target is irrelevant. Subsequent contacts receive no recovery
once the charge is exhausted. A multi-charge application consumes one charge
per successful contact.

刺目, incoming guaranteed misses, and dodges resolve before this behavior, so
missed contacts neither restore MP nor consume 化勁. MP recovery follows the
normal MP-recovery modifiers and maximum-MP cap. Reapplication replaces the
complete existing packet and its bound formula.

```yaml
套用狀態:
  狀態: 化勁
  可觸發次數: 1
  命中回內:
    基準: 來源星級
    固定: 40
    百分比: 1000
```

`命中回內` replaces `化解後護盾`; the previous field is no longer accepted.
The catalog creates a `HitBeforeDamage` rule observed on the status holder as
attack source, selecting `HitTarget`, with MP restoration followed by
`ConsumeThisStatusAction`. The source-star formula is bound when the status is
applied, using the producer's star rather than the afflicted attacker's star.
No cast suppression or shield action belongs to 化勁.
