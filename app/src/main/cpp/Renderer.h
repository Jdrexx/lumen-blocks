#ifndef GAME_TEST_RENDERER_H
#define GAME_TEST_RENDERER_H

#include <EGL/egl.h>
#include <GLES3/gl3.h>

#include <array>
#include <chrono>
#include <cstdint>
#include <random>
#include <string>
#include <vector>

struct android_app;

class Renderer {
public:
    struct Cell {
        int x;
        int y;
    };

    struct Piece {
        std::vector<Cell> cells;
        int color = 0;
        int special = 0;
        bool used = false;
    };

    struct Color {
        float r;
        float g;
        float b;
        float a;
    };

    explicit Renderer(android_app *app);
    ~Renderer();

    void handleInput();
    void render();

private:
    enum class Screen { Home, Playing, Tutorial, Settings, Stats, Achievements };
    enum class Mode { Classic, Journey, Daily, Zen, Rush };

    struct Profile {
        std::array<int, 5> bestScores{};
        std::array<int, 5> topScores{};
        int lumen = 0;
        int journeyStage = 0;
        int totalLines = 0;
        int totalPieces = 0;
        int gamesPlayed = 0;
        int maxCombo = 0;
        int dailyBest = 0;
        bool tutorialSeen = false;
        bool soundEnabled = true;
        bool hapticsEnabled = true;
        bool highContrast = false;
    };

    void initRenderer();
    void updateRenderArea();
    void startGame(Mode mode);
    void finishGame();
    void resetGame();
    void refillTray();
    Piece randomPiece();
    bool trayHasMove() const;

    void loadProfile();
    void saveProfile() const;
    void addTopScore(int score);
    void sendFeedback(int kind) const;
    void shareScore() const;
    int modeIndex() const;
    int objectiveTarget() const;
    int restorationLevel() const;
    std::string restorationName() const;

    void pointerDown(float x, float y);
    void pointerMove(float x, float y);
    void pointerUp(float x, float y);
    int traySlotAt(float x, float y) const;
    bool dragPlacement(int &column, int &row) const;
    bool canPlace(const Piece &piece, int column, int row) const;
    bool hasAnyMove() const;
    void placeDraggedPiece();
    void clearCompletedLines();

    void drawScene();
    void drawHome();
    void drawGame();
    void drawTutorial();
    void drawSettings();
    void drawStats();
    void drawAchievements();
    void drawButton(float left, float top, float right, float bottom,
                    const std::string &label, const Color &color);
    void drawRect(float left, float top, float right, float bottom, const Color &color);
    void drawCell(float left, float top, float size, const Color &color, float alpha = 1.0f);
    void drawPiece(const Piece &piece, float originX, float originY, float cellSize,
                   float alpha = 1.0f);
    void drawNumber(int value, float centerX, float top, float digitWidth, const Color &color);
    void drawDigit(int digit, float left, float top, float width, const Color &color);
    void drawText(const std::string &text, float centerX, float top, float size,
                  const Color &color);
    void drawGlyph(char glyph, float left, float top, float size, const Color &color);
    float cellHeight(float width) const;

    android_app *app_;
    EGLDisplay display_ = EGL_NO_DISPLAY;
    EGLSurface surface_ = EGL_NO_SURFACE;
    EGLContext context_ = EGL_NO_CONTEXT;
    EGLint width_ = 0;
    EGLint height_ = 0;

    GLuint program_ = 0;
    GLuint vertexBuffer_ = 0;
    GLuint vertexArray_ = 0;
    GLint positionLocation_ = -1;
    GLint colorLocation_ = -1;

    std::array<std::array<int, 8>, 8> board_{};
    std::array<Piece, 3> tray_;
    std::mt19937 random_;
    Profile profile_;
    Screen screen_ = Screen::Home;
    Mode mode_ = Mode::Classic;

    int score_ = 0;
    int bestScore_ = 0;
    int combo_ = 0;
    int linesThisRun_ = 0;
    int piecesThisRun_ = 0;
    int shufflesLeft_ = 1;
    int heldSlot_ = -1;
    int lastClearCount_ = 0;
    float feedbackTimer_ = 0.0f;
    float pulseTimer_ = 0.0f;
    float rushSeconds_ = 180.0f;
    std::chrono::steady_clock::time_point lastFrame_;
    bool gameOver_ = false;
    bool paused_ = false;

    int activePointerId_ = -1;
    int draggedSlot_ = -1;
    float touchX_ = 0.0f;
    float touchY_ = 0.0f;
};

#endif
