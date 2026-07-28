# Google Play Games Services setup

Lumen Blocks uses Google Play Games Services v2 (`play-services-games-v2`) for
automatic platform authentication, five global leaderboards, and six
achievements. Offline play and local progress remain available when the player
is signed out or has no network connection.

## 1. Choose the permanent package name

The permanent application ID is `com.jdrexx.lumenblocks`. Register this exact
value in Play Console and use it for both debug and release Android credentials.

Changing the application ID after a Play Store release creates a different app.

## 2. Create the Play Console game

1. Create the Android app in Google Play Console and mark it as a game.
2. Open **Grow users > Play Games Services > Setup and management >
   Configuration**.
3. Create or link a Google Cloud project and enable the Google Play Game
   Services API.
4. Configure the OAuth consent screen.
5. Create the Play Games Services project and record its numeric project ID.

## 3. Link Android credentials

Create two Android credentials with the same permanent package name:

- A debug credential using the debug keystore SHA-1 for local testing.
- A release credential using the Play App Signing certificate SHA-1.

Get the local debug fingerprint with:

```bash
./gradlew signingReport
```

The current development machine's debug SHA-1 is:

```text
5C:AE:26:65:A2:DA:49:48:61:B5:6A:AC:3B:7B:FC:81:B2:12:D4:FF
```

In Play Console, add tester Gmail accounts before testing an unpublished game.
Authentication fails when either the package name or signing certificate does
not match a linked credential.

## 4. Define Play Games features

Create these **larger is better** leaderboards:

- Classic
- Journey
- Daily
- Zen
- Rush

Create these standard achievements:

- First Light — clear 1 line
- Line Keeper — clear 50 total lines
- Architect — place 250 total pieces
- Combo Five — reach a 5× combo
- Bright World — earn 1,000 Lumen
- Master — score 5,000 in Classic

Publish the Play Games Services configuration to testers after creating the
features. This is separate from publishing the Android app.

## 5. Install generated resource IDs

Replace the project, leaderboard, and achievement placeholders in
`app/src/main/res/values/strings.xml` with the exact IDs exported by Play
Console. Keep IDs in resources; do not copy them into Kotlin or C++ code.

## 6. Enable and test

Add this line to the root `gradle.properties`:

```properties
PLAY_GAMES_ENABLED=true
```

Then install a build signed by one of the linked certificates:

```bash
./gradlew assembleDebug
```

On startup, Play Games v2 attempts automatic authentication. Finish a run to
submit its score and synchronize earned achievements. Use **Global** on the
Statistics screen and **Google** on the Achievements screen to verify the native
Play Games interfaces.

Do not enable the flag while placeholder IDs remain. The default value is
`false`, so fresh clones and CI builds remain safe and fully playable offline.
