# Lumen Blocks

Lumen Blocks is an offline block-placement puzzle game for Android. Drag colorful
pieces onto an 8×8 board, complete rows and columns, build scoring combos, and
keep the board open for as long as possible.

The game is an original mobile puzzle project built with Kotlin, Android
GameActivity, C++, and OpenGL ES 3.

## How to play

1. Each turn presents three pieces in the tray at the bottom of the screen.
2. Drag a piece from the tray onto any valid open cells on the board.
3. A piece cannot overlap an occupied cell or extend beyond the board.
4. Completely fill a horizontal row or vertical column to clear it.
5. Multiple completed lines clear together and award a larger bonus.
6. Clear lines on consecutive placements to build a combo.
7. After all three tray pieces are used, three new pieces appear.
8. The run ends when none of the remaining pieces can fit anywhere.

There is no time limit. The goal is to plan ahead, preserve flexible space, and
earn the highest score possible.

## Scoring

- Every placed block cell awards **10 points**.
- Every cleared row or column awards a base **100 points**.
- Clearing several lines at once applies a squared multi-line bonus.
- Consecutive scoring placements add a **50-point combo bonus** per combo level.
- A placement that clears no lines resets the combo.

## Controls

- **Pick up a piece:** touch a piece in the bottom tray.
- **Move a piece:** drag it toward the board.
- **Placement preview:** green means the position is valid; red means it is not.
- **Place a piece:** release your finger over a valid position.
- **Restart:** tap the game-over panel after the board has no remaining moves.

## Current features

- Native 8×8 puzzle board
- Randomized one- to nine-cell pieces
- Touch-first drag-and-drop controls
- Valid and invalid placement previews
- Simultaneous row and column clearing
- Multi-line and consecutive-clear bonuses
- Current-score and best-session displays
- Portrait, immersive full-screen presentation
- Offline play with no account or network requirement
- Support for Android 8.0 (API 26) and newer

## Technology

- Kotlin activity shell
- Android GameActivity
- C++ game rules and input handling
- OpenGL ES 3 rendering
- CMake and Android NDK
- Gradle Kotlin DSL

The main game implementation is located in
[`app/src/main/cpp/Renderer.cpp`](app/src/main/cpp/Renderer.cpp).

## Build and run

### Android Studio

1. Open the repository in a current version of Android Studio.
2. Allow Gradle to synchronize the project.
3. Select an Android emulator or connected device.
4. Press **Run**.

### Command line

Set `JAVA_HOME` to a compatible JDK and run:

```bash
./gradlew assembleDebug
```

The debug APK will be generated at:

```text
app/build/outputs/apk/debug/app-debug.apk
```

Run the unit tests with:

```bash
./gradlew testDebugUnitTest
```

## Project status

Lumen Blocks is currently an early playable prototype. Planned improvements
include persistent high scores, sound and haptic feedback, line-clear
animations, daily challenges, accessibility settings, and a fuller restoration
progression system.

