# Google Play Data Safety working notes

These notes are a starting point for the Play Console Data safety form. The
final answers must match the exact release artifact, enabled Play Games
configuration, target audience, and any SDKs added later.

## Current app behavior

- Local scores, progress, statistics, achievements, and settings stay on-device.
- No advertising SDK is included.
- No developer-operated analytics or backend is included.
- No sensitive runtime permissions are requested by the app manifest.
- Google Play Games Services v2 is included for optional authentication,
  leaderboards, and achievements.
- The Android share sheet is used only after a user taps Share.

## Likely Play Console disclosures when Play Games is enabled

Google Play treats SDK transmissions as collection by the app for Data safety
purposes. Review the current Google Play Games SDK disclosure before submission.
Potential categories include:

- **Personal info / User IDs:** Play Games gamer identity, for account
  management and app functionality.
- **App activity / Other user-generated content:** scores, achievements, and
  gameplay progress submitted to Play Games, for app functionality.
- **App info and performance / Device or other IDs:** information processed by
  Google Play services for functionality, fraud prevention, analytics, and
  troubleshooting.
- **Approximate location:** Google states that Play Games may infer a country or
  region, including from an IP address. Confirm how the Data safety form
  classifies the current SDK behavior.

Data sent to Google services should be declared according to the form's current
definitions of collection, sharing, optionality, purpose, encryption in transit,
and deletion. Do not select "no data collected" merely because Jdrexx does not
operate a server.

## Retention and deletion

- Local data: retained until cleared, overwritten, or the app is uninstalled;
  device backups may retain a copy.
- Google data: governed by Google Play Games and Google account controls.
- Developer inquiries: retained only as reasonably necessary to respond and
  comply with legal obligations.

## Required store assets

- Public privacy-policy URL pointing to `PRIVACY_POLICY.md` or a hosted copy.
- Matching privacy-policy text or link accessible within the app before
  production release.
- Accurate Data safety form.
- Accurate target-audience and content-rating declarations.

Re-review this document whenever an advertising, analytics, crash-reporting,
cloud-save, account, payment, or backend SDK is added.
