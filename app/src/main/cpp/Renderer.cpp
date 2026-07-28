#include "Renderer.h"

#include <android/input.h>
#include <algorithm>
#include <cassert>
#include <chrono>
#include <cctype>
#include <cmath>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <game-activity/GameActivity.h>
#include <game-activity/native_app_glue/android_native_app_glue.h>

#include "AndroidOut.h"

namespace {
constexpr int kBoardSize = 8;
constexpr float kBoardLeft = 0.08f;
constexpr float kBoardTop = 0.24f;
constexpr float kBoardWidth = 0.84f;
constexpr float kCell = kBoardWidth / kBoardSize;
constexpr float kTrayTop = 0.79f;
constexpr float kTrayHeight = 0.18f;

constexpr Renderer::Color kBackground{0.035f, 0.055f, 0.105f, 1.0f};
constexpr Renderer::Color kPanel{0.065f, 0.095f, 0.165f, 1.0f};
constexpr Renderer::Color kEmptyCell{0.105f, 0.145f, 0.22f, 1.0f};
constexpr Renderer::Color kText{0.92f, 0.96f, 1.0f, 1.0f};
constexpr Renderer::Color kAccent{0.35f, 0.95f, 0.77f, 1.0f};
constexpr Renderer::Color kDanger{1.0f, 0.34f, 0.42f, 1.0f};

constexpr std::array<Renderer::Color, 7> kBlockColors{{
        {0.29f, 0.82f, 1.0f, 1.0f},
        {0.43f, 0.95f, 0.55f, 1.0f},
        {1.0f, 0.72f, 0.26f, 1.0f},
        {0.76f, 0.48f, 1.0f, 1.0f},
        {1.0f, 0.40f, 0.60f, 1.0f},
        {0.98f, 0.91f, 0.32f, 1.0f},
        {0.28f, 0.62f, 1.0f, 1.0f},
}};

GLuint compileShader(GLenum type, const char *source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    GLint compiled = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (compiled != GL_TRUE) {
        GLint length = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
        std::vector<char> log(static_cast<size_t>(length));
        glGetShaderInfoLog(shader, length, nullptr, log.data());
        aout << "Shader compile failed: " << log.data() << std::endl;
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

int pieceWidth(const Renderer::Piece &piece) {
    int maximum = 0;
    for (const auto &cell : piece.cells) maximum = std::max(maximum, cell.x);
    return maximum + 1;
}

int pieceHeight(const Renderer::Piece &piece) {
    int maximum = 0;
    for (const auto &cell : piece.cells) maximum = std::max(maximum, cell.y);
    return maximum + 1;
}
}  // namespace

Renderer::Renderer(android_app *app)
        : app_(app),
          random_(static_cast<unsigned int>(
                  std::chrono::steady_clock::now().time_since_epoch().count())) {
    initRenderer();
    loadProfile();
    lastFrame_ = std::chrono::steady_clock::now();
    screen_ = profile_.tutorialSeen ? Screen::Home : Screen::Tutorial;
}

Renderer::~Renderer() {
    if (display_ != EGL_NO_DISPLAY) {
        eglMakeCurrent(display_, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (program_) glDeleteProgram(program_);
        if (vertexBuffer_) glDeleteBuffers(1, &vertexBuffer_);
        if (vertexArray_) glDeleteVertexArrays(1, &vertexArray_);
        if (context_ != EGL_NO_CONTEXT) eglDestroyContext(display_, context_);
        if (surface_ != EGL_NO_SURFACE) eglDestroySurface(display_, surface_);
        eglTerminate(display_);
    }
}

void Renderer::initRenderer() {
    constexpr EGLint attributes[] = {
            EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
            EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
            EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8,
            EGL_ALPHA_SIZE, 8, EGL_NONE};

    display_ = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    eglInitialize(display_, nullptr, nullptr);
    EGLConfig config;
    EGLint count;
    eglChooseConfig(display_, attributes, &config, 1, &count);
    surface_ = eglCreateWindowSurface(display_, config, app_->window, nullptr);
    constexpr EGLint contextAttributes[] = {EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE};
    context_ = eglCreateContext(display_, config, EGL_NO_CONTEXT, contextAttributes);
    assert(eglMakeCurrent(display_, surface_, surface_, context_) == EGL_TRUE);

    constexpr const char *vertexSource = R"(#version 300 es
        layout(location = 0) in vec2 inPosition;
        void main() {
            gl_Position = vec4(inPosition.x * 2.0 - 1.0, 1.0 - inPosition.y * 2.0, 0.0, 1.0);
        }
    )";
    constexpr const char *fragmentSource = R"(#version 300 es
        precision mediump float;
        uniform vec4 color;
        out vec4 outColor;
        void main() { outColor = color; }
    )";

    GLuint vertexShader = compileShader(GL_VERTEX_SHADER, vertexSource);
    GLuint fragmentShader = compileShader(GL_FRAGMENT_SHADER, fragmentSource);
    program_ = glCreateProgram();
    glAttachShader(program_, vertexShader);
    glAttachShader(program_, fragmentShader);
    glLinkProgram(program_);
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    GLint linked = GL_FALSE;
    glGetProgramiv(program_, GL_LINK_STATUS, &linked);
    assert(linked == GL_TRUE);
    positionLocation_ = 0;
    colorLocation_ = glGetUniformLocation(program_, "color");

    glGenVertexArrays(1, &vertexArray_);
    glBindVertexArray(vertexArray_);
    glGenBuffers(1, &vertexBuffer_);
    glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 12, nullptr, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(positionLocation_, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
    glEnableVertexAttribArray(positionLocation_);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glClearColor(kBackground.r, kBackground.g, kBackground.b, 1.0f);
    eglSwapInterval(display_, 1);
    updateRenderArea();
}

void Renderer::updateRenderArea() {
    EGLint width;
    EGLint height;
    eglQuerySurface(display_, surface_, EGL_WIDTH, &width);
    eglQuerySurface(display_, surface_, EGL_HEIGHT, &height);
    if (width != width_ || height != height_) {
        width_ = width;
        height_ = height;
        glViewport(0, 0, width_, height_);
    }
}

void Renderer::startGame(Mode mode) {
    mode_ = mode;
    if (mode_ == Mode::Daily) {
        const std::time_t now = std::time(nullptr);
        std::tm calendar{};
        localtime_r(&now, &calendar);
        const unsigned int seed = static_cast<unsigned int>(
                (calendar.tm_year + 1900) * 10000 + (calendar.tm_mon + 1) * 100 +
                calendar.tm_mday);
        random_.seed(seed);
    } else {
        random_.seed(static_cast<unsigned int>(
                std::chrono::steady_clock::now().time_since_epoch().count()));
    }
    resetGame();
    screen_ = Screen::Playing;
}

void Renderer::resetGame() {
    for (auto &row : board_) row.fill(0);
    score_ = 0;
    combo_ = 0;
    linesThisRun_ = 0;
    piecesThisRun_ = 0;
    shufflesLeft_ = 1;
    heldSlot_ = -1;
    lastClearCount_ = 0;
    feedbackTimer_ = 0.0f;
    rushSeconds_ = 180.0f;
    gameOver_ = false;
    paused_ = false;
    draggedSlot_ = -1;
    bestScore_ = profile_.bestScores[modeIndex()];
    refillTray();
}

Renderer::Piece Renderer::randomPiece() {
    static const std::vector<std::vector<Cell>> shapes = {
            {{0, 0}}, {{0, 0}, {1, 0}}, {{0, 0}, {0, 1}},
            {{0, 0}, {1, 0}, {2, 0}}, {{0, 0}, {0, 1}, {0, 2}},
            {{0, 0}, {1, 0}, {0, 1}}, {{0, 0}, {1, 0}, {1, 1}},
            {{0, 0}, {0, 1}, {1, 1}}, {{1, 0}, {0, 1}, {1, 1}},
            {{0, 0}, {1, 0}, {0, 1}, {1, 1}},
            {{0, 0}, {1, 0}, {2, 0}, {3, 0}},
            {{0, 0}, {0, 1}, {0, 2}, {0, 3}},
            {{0, 0}, {1, 0}, {2, 0}, {1, 1}},
            {{0, 0}, {0, 1}, {1, 1}, {1, 2}},
            {{1, 0}, {0, 1}, {1, 1}, {0, 2}},
            {{0, 0}, {1, 0}, {2, 0}, {0, 1}, {1, 1}, {2, 1},
             {0, 2}, {1, 2}, {2, 2}},
    };
    std::uniform_int_distribution<size_t> shapeDistribution(0, shapes.size() - 1);
    std::uniform_int_distribution<int> colorDistribution(0, kBlockColors.size() - 1);
    std::uniform_int_distribution<int> specialRoll(0, 99);
    int special = 0;
    if (profile_.lumen >= 300 && specialRoll(random_) < 6) special = 1;  // Prism.
    if (profile_.lumen >= 900 && specialRoll(random_) < 4) special = 2;  // Lantern.
    return {shapes[shapeDistribution(random_)], colorDistribution(random_) + 1, special, false};
}

void Renderer::refillTray() {
    for (auto &piece : tray_) piece = randomPiece();
    // Fairness pass: never issue a completely dead tray when a smaller piece can fit.
    if (!trayHasMove()) {
        tray_[0] = {{{0, 0}}, 1, 0, false};
    }
}

int Renderer::modeIndex() const {
    return static_cast<int>(mode_);
}

int Renderer::objectiveTarget() const {
    return 8 + profile_.journeyStage * 2;
}

int Renderer::restorationLevel() const {
    return std::min(19, profile_.lumen / 250);
}

std::string Renderer::restorationName() const {
    static const std::array<const char *, 5> locations{
            "MOONLIT GARDEN", "SUNKEN LIBRARY", "CRYSTAL OBSERVATORY",
            "FORGOTTEN CITY", "CELESTIAL TEMPLE"};
    return locations[static_cast<size_t>(restorationLevel() / 4)];
}

void Renderer::loadProfile() {
    std::ifstream input(std::string(app_->activity->internalDataPath) + "/lumen.profile");
    if (!input) return;
    int tutorial = 0;
    int sound = 1;
    int haptics = 1;
    int contrast = 0;
    input >> profile_.lumen >> profile_.journeyStage >> profile_.totalLines
          >> profile_.totalPieces >> profile_.gamesPlayed >> profile_.maxCombo
          >> profile_.dailyBest >> tutorial >> sound >> haptics >> contrast;
    for (int &score : profile_.bestScores) input >> score;
    for (int &score : profile_.topScores) input >> score;
    profile_.tutorialSeen = tutorial != 0;
    profile_.soundEnabled = sound != 0;
    profile_.hapticsEnabled = haptics != 0;
    profile_.highContrast = contrast != 0;
}

void Renderer::saveProfile() const {
    std::ofstream output(std::string(app_->activity->internalDataPath) + "/lumen.profile",
                         std::ios::trunc);
    output << profile_.lumen << ' ' << profile_.journeyStage << ' '
           << profile_.totalLines << ' ' << profile_.totalPieces << ' '
           << profile_.gamesPlayed << ' ' << profile_.maxCombo << ' '
           << profile_.dailyBest << ' ' << profile_.tutorialSeen << ' '
           << profile_.soundEnabled << ' ' << profile_.hapticsEnabled << ' '
           << profile_.highContrast;
    for (int score : profile_.bestScores) output << ' ' << score;
    for (int score : profile_.topScores) output << ' ' << score;
    output << '\n';
}

void Renderer::addTopScore(int score) {
    profile_.topScores.back() = std::max(profile_.topScores.back(), score);
    std::sort(profile_.topScores.begin(), profile_.topScores.end(), std::greater<>());
}

void Renderer::finishGame() {
    if (gameOver_) return;
    gameOver_ = true;
    ++profile_.gamesPlayed;
    profile_.bestScores[modeIndex()] =
            std::max(profile_.bestScores[modeIndex()], score_);
    if (mode_ == Mode::Daily) profile_.dailyBest = std::max(profile_.dailyBest, score_);
    addTopScore(score_);
    saveProfile();
    syncPlayGames();
}

void Renderer::sendFeedback(int kind) const {
    JNIEnv *environment = nullptr;
    bool detach = false;
    if (app_->activity->vm->GetEnv(reinterpret_cast<void **>(&environment), JNI_VERSION_1_6) !=
        JNI_OK) {
        if (app_->activity->vm->AttachCurrentThread(&environment, nullptr) != JNI_OK) return;
        detach = true;
    }
    jclass activityClass = environment->GetObjectClass(app_->activity->javaGameActivity);
    jmethodID method = environment->GetMethodID(activityClass, "gameFeedback", "(IZZ)V");
    if (method) {
        environment->CallVoidMethod(app_->activity->javaGameActivity, method, kind,
                                    profile_.soundEnabled, profile_.hapticsEnabled);
    }
    environment->DeleteLocalRef(activityClass);
    if (detach) app_->activity->vm->DetachCurrentThread();
}

void Renderer::shareScore() const {
    JNIEnv *environment = nullptr;
    bool detach = false;
    if (app_->activity->vm->GetEnv(reinterpret_cast<void **>(&environment), JNI_VERSION_1_6) !=
        JNI_OK) {
        if (app_->activity->vm->AttachCurrentThread(&environment, nullptr) != JNI_OK) return;
        detach = true;
    }
    jclass activityClass = environment->GetObjectClass(app_->activity->javaGameActivity);
    jmethodID method = environment->GetMethodID(activityClass, "shareScore", "(II)V");
    if (method) {
        environment->CallVoidMethod(app_->activity->javaGameActivity, method, score_, modeIndex());
    }
    environment->DeleteLocalRef(activityClass);
    if (detach) app_->activity->vm->DetachCurrentThread();
}

void Renderer::syncPlayGames() const {
    JNIEnv *environment = nullptr;
    bool detach = false;
    if (app_->activity->vm->GetEnv(reinterpret_cast<void **>(&environment), JNI_VERSION_1_6) !=
        JNI_OK) {
        if (app_->activity->vm->AttachCurrentThread(&environment, nullptr) != JNI_OK) return;
        detach = true;
    }
    jclass activityClass = environment->GetObjectClass(app_->activity->javaGameActivity);
    jmethodID method = environment->GetMethodID(activityClass, "syncPlayGames", "(IIIIII)V");
    if (method) {
        environment->CallVoidMethod(
                app_->activity->javaGameActivity, method, score_, modeIndex(),
                profile_.totalLines, profile_.totalPieces, profile_.maxCombo, profile_.lumen);
    }
    environment->DeleteLocalRef(activityClass);
    if (detach) app_->activity->vm->DetachCurrentThread();
}

void Renderer::showPlayGames(int view) const {
    JNIEnv *environment = nullptr;
    bool detach = false;
    if (app_->activity->vm->GetEnv(reinterpret_cast<void **>(&environment), JNI_VERSION_1_6) !=
        JNI_OK) {
        if (app_->activity->vm->AttachCurrentThread(&environment, nullptr) != JNI_OK) return;
        detach = true;
    }
    jclass activityClass = environment->GetObjectClass(app_->activity->javaGameActivity);
    jmethodID method = environment->GetMethodID(activityClass, "showPlayGames", "(I)V");
    if (method) {
        environment->CallVoidMethod(app_->activity->javaGameActivity, method, view);
    }
    environment->DeleteLocalRef(activityClass);
    if (detach) app_->activity->vm->DetachCurrentThread();
}

void Renderer::openPrivacyPolicy() const {
    JNIEnv *environment = nullptr;
    bool detach = false;
    if (app_->activity->vm->GetEnv(reinterpret_cast<void **>(&environment), JNI_VERSION_1_6) !=
        JNI_OK) {
        if (app_->activity->vm->AttachCurrentThread(&environment, nullptr) != JNI_OK) return;
        detach = true;
    }
    jclass activityClass = environment->GetObjectClass(app_->activity->javaGameActivity);
    jmethodID method = environment->GetMethodID(activityClass, "openPrivacyPolicy", "()V");
    if (method) {
        environment->CallVoidMethod(app_->activity->javaGameActivity, method);
    }
    environment->DeleteLocalRef(activityClass);
    if (detach) app_->activity->vm->DetachCurrentThread();
}

void Renderer::render() {
    updateRenderArea();
    const auto now = std::chrono::steady_clock::now();
    const float delta = std::min(0.05f, std::chrono::duration<float>(now - lastFrame_).count());
    lastFrame_ = now;
    feedbackTimer_ = std::max(0.0f, feedbackTimer_ - delta);
    pulseTimer_ += delta;
    if (screen_ == Screen::Playing && mode_ == Mode::Rush && !paused_ && !gameOver_) {
        rushSeconds_ = std::max(0.0f, rushSeconds_ - delta);
        if (rushSeconds_ <= 0.0f) finishGame();
    }
    glClear(GL_COLOR_BUFFER_BIT);
    drawScene();
    eglSwapBuffers(display_, surface_);
}

void Renderer::drawRect(float left, float top, float right, float bottom, const Color &color) {
    const float vertices[] = {
            left, top, right, top, right, bottom,
            left, top, right, bottom, left, bottom,
    };
    glUseProgram(program_);
    glBindVertexArray(vertexArray_);
    glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer_);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);
    glUniform4f(colorLocation_, color.r, color.g, color.b, color.a);
    glDrawArrays(GL_TRIANGLES, 0, 6);
}

void Renderer::drawCell(float left, float top, float size, const Color &color, float alpha) {
    const float height = cellHeight(size);
    Color shadow{color.r * 0.55f, color.g * 0.55f, color.b * 0.55f, alpha};
    Color face{color.r, color.g, color.b, alpha};
    const float gapX = size * 0.075f;
    const float gapY = height * 0.075f;
    drawRect(left + gapX, top + gapY, left + size - gapX, top + height - gapY, shadow);
    drawRect(left + gapX, top + gapY, left + size - gapX, top + height * 0.72f, face);
    Color shine{1.0f, 1.0f, 1.0f, alpha * 0.2f};
    drawRect(left + size * 0.18f, top + height * 0.16f,
             left + size * 0.72f, top + height * 0.22f, shine);
}

float Renderer::cellHeight(float width) const {
    if (height_ <= 0) return width;
    return width * static_cast<float>(width_) / static_cast<float>(height_);
}

void Renderer::drawPiece(const Piece &piece, float originX, float originY, float cellSize,
                         float alpha) {
    Color color = kBlockColors[static_cast<size_t>(piece.color - 1)];
    if (piece.special == 1) color = {0.92f, 0.92f, 1.0f, 1.0f};
    if (piece.special == 2) color = {1.0f, 0.52f, 0.15f, 1.0f};
    for (const auto &cell : piece.cells) {
        drawCell(originX + cell.x * cellSize, originY + cell.y * cellHeight(cellSize),
                 cellSize, color, alpha);
    }
}

void Renderer::drawScene() {
    switch (screen_) {
        case Screen::Home: drawHome(); break;
        case Screen::Playing: drawGame(); break;
        case Screen::Tutorial: drawTutorial(); break;
        case Screen::Settings: drawSettings(); break;
        case Screen::Stats: drawStats(); break;
        case Screen::Achievements: drawAchievements(); break;
    }
}

void Renderer::drawGame() {
    drawRect(0.0f, 0.0f, 1.0f, 1.0f, kBackground);

    // Original "restoration" identity: a horizon that brightens with the score.
    const float progress = std::min(1.0f, score_ / 1200.0f);
    Color glow{0.12f + progress * 0.20f, 0.12f + progress * 0.30f,
               0.25f + progress * 0.35f, 1.0f};
    drawRect(0.0f, 0.0f, 1.0f, 0.205f, glow);
    drawRect(0.0f, 0.18f, 1.0f, 0.205f, kAccent);

    static const std::array<const char *, 5> modeNames{
            "CLASSIC", "JOURNEY", "DAILY", "ZEN", "RUSH"};
    drawText(modeNames[modeIndex()], 0.16f, 0.025f, 0.012f, kAccent);
    drawText("SCORE", 0.50f, 0.020f, 0.010f, kText);
    drawNumber(score_, 0.50f, 0.055f, 0.055f, kText);
    drawText("BEST", 0.87f, 0.040f, 0.009f, kAccent);
    drawNumber(bestScore_, 0.87f, 0.075f, 0.026f, kAccent);
    drawButton(0.025f, 0.135f, 0.13f, 0.175f, "HOME", kPanel);
    drawButton(0.87f, 0.135f, 0.975f, 0.175f, paused_ ? "PLAY" : "PAUSE", kPanel);
    if (mode_ == Mode::Rush) {
        drawNumber(static_cast<int>(std::ceil(rushSeconds_)), 0.72f, 0.14f, 0.018f, kDanger);
    }
    if (mode_ == Mode::Journey) {
        drawText("LINES", 0.30f, 0.145f, 0.008f, kText);
        drawNumber(linesThisRun_, 0.41f, 0.14f, 0.018f, kAccent);
        drawNumber(objectiveTarget(), 0.54f, 0.14f, 0.018f, kText);
    }

    const float boardHeight = cellHeight(kBoardWidth);
    drawRect(kBoardLeft - 0.012f, kBoardTop - 0.008f,
             kBoardLeft + kBoardWidth + 0.012f,
             kBoardTop + boardHeight + 0.008f, kPanel);
    for (int row = 0; row < kBoardSize; ++row) {
        for (int column = 0; column < kBoardSize; ++column) {
            const float left = kBoardLeft + column * kCell;
            const float top = kBoardTop + row * cellHeight(kCell);
            if (board_[row][column] == 0) {
                drawCell(left, top, kCell, profile_.highContrast
                        ? Color{0.22f, 0.27f, 0.36f, 1.0f} : kEmptyCell);
            } else {
                drawCell(left, top, kCell,
                         kBlockColors[static_cast<size_t>(board_[row][column] - 1)]);
            }
        }
    }

    if (draggedSlot_ >= 0) {
        int column;
        int row;
        if (dragPlacement(column, row)) {
            const bool valid = canPlace(tray_[draggedSlot_], column, row);
            for (const auto &cell : tray_[draggedSlot_].cells) {
                if (column + cell.x >= 0 && column + cell.x < kBoardSize &&
                    row + cell.y >= 0 && row + cell.y < kBoardSize) {
                    drawCell(kBoardLeft + (column + cell.x) * kCell,
                             kBoardTop + (row + cell.y) * cellHeight(kCell), kCell,
                             valid ? kAccent : kDanger, 0.60f);
                }
            }
        }
    }

    drawRect(0.04f, kTrayTop - 0.015f, 0.96f, kTrayTop + kTrayHeight, kPanel);
    if (shufflesLeft_ > 0 && !gameOver_ && !paused_) {
        drawButton(0.73f, 0.705f, 0.95f, 0.75f, "SHUFFLE", kPanel);
    }
    for (int slot = 0; slot < 3; ++slot) {
        if (tray_[slot].used || slot == draggedSlot_) continue;
        const float trayCell = 0.045f;
        const float center = (slot + 0.5f) / 3.0f;
        const float originX = center - pieceWidth(tray_[slot]) * trayCell * 0.5f;
        const float originY = kTrayTop + 0.065f -
                pieceHeight(tray_[slot]) * cellHeight(trayCell) * 0.5f;
        drawPiece(tray_[slot], originX, originY, trayCell);
    }

    if (draggedSlot_ >= 0) {
        const Piece &piece = tray_[draggedSlot_];
        drawPiece(piece, touchX_ - pieceWidth(piece) * kCell * 0.5f,
                  touchY_ - 0.10f, kCell, 0.92f);
    }

    if (gameOver_) {
        drawRect(0.08f, 0.39f, 0.92f, 0.64f, {0.025f, 0.035f, 0.075f, 0.96f});
        drawText(mode_ == Mode::Rush ? "TIME" : "NO MOVES", 0.50f, 0.415f,
                 0.018f, kDanger);
        drawNumber(score_, 0.50f, 0.45f, 0.065f, kDanger);
        drawButton(0.12f, 0.565f, 0.48f, 0.615f, "AGAIN", kAccent);
        drawButton(0.52f, 0.565f, 0.88f, 0.615f, "SHARE", kPanel);
    } else if (paused_) {
        drawRect(0.08f, 0.39f, 0.92f, 0.61f, {0.025f, 0.035f, 0.075f, 0.96f});
        drawText("PAUSED", 0.50f, 0.45f, 0.025f, kText);
        drawText("TAP PLAY TO CONTINUE", 0.50f, 0.53f, 0.009f, kAccent);
    } else if (feedbackTimer_ > 0.0f && lastClearCount_ > 0) {
        drawText(combo_ > 1 ? "COMBO" : "CLEAR", 0.50f, 0.69f,
                 0.016f + 0.003f * std::sin(pulseTimer_ * 12.0f), kAccent);
        drawNumber(lastClearCount_, 0.63f, 0.685f, 0.022f, kText);
    }
}

void Renderer::drawButton(float left, float top, float right, float bottom,
                          const std::string &label, const Color &color) {
    drawRect(left, top, right, bottom, color);
    drawText(label, (left + right) * 0.5f, top + (bottom - top) * 0.32f,
             std::min(0.012f, (right - left) / std::max(8.0f, label.size() * 6.0f)), kText);
}

void Renderer::drawHome() {
    const int level = restorationLevel();
    const float restored = (profile_.lumen % 250) / 250.0f;
    Color sky{0.06f + level * 0.012f, 0.08f + level * 0.016f,
              0.16f + level * 0.018f, 1.0f};
    drawRect(0, 0, 1, 1, sky);
    drawText("LUMEN BLOCKS", 0.50f, 0.065f, 0.028f, kText);
    drawText(restorationName(), 0.50f, 0.135f, 0.012f, kAccent);
    drawText("LUMEN", 0.28f, 0.185f, 0.010f, kText);
    drawNumber(profile_.lumen, 0.58f, 0.172f, 0.026f, kAccent);
    drawRect(0.12f, 0.23f, 0.88f, 0.25f, kPanel);
    drawRect(0.12f, 0.23f, 0.12f + 0.76f * restored, 0.25f, kAccent);

    drawButton(0.14f, 0.31f, 0.86f, 0.37f, "CLASSIC", {0.15f, 0.42f, 0.60f, 1});
    drawButton(0.14f, 0.39f, 0.86f, 0.45f, "JOURNEY", {0.20f, 0.50f, 0.42f, 1});
    drawButton(0.14f, 0.47f, 0.48f, 0.53f, "DAILY", {0.45f, 0.30f, 0.60f, 1});
    drawButton(0.52f, 0.47f, 0.86f, 0.53f, "ZEN", {0.22f, 0.45f, 0.54f, 1});
    drawButton(0.14f, 0.55f, 0.86f, 0.61f, "RUSH", {0.62f, 0.25f, 0.34f, 1});
    drawButton(0.08f, 0.72f, 0.37f, 0.77f, "HOW TO", kPanel);
    drawButton(0.40f, 0.72f, 0.68f, 0.77f, "STATS", kPanel);
    drawButton(0.71f, 0.72f, 0.92f, 0.77f, "SET", kPanel);
    drawButton(0.20f, 0.80f, 0.80f, 0.85f, "ACHIEVEMENTS", kPanel);
    drawText("RESTORE THE LOST LIGHT", 0.50f, 0.92f, 0.010f, kAccent);
}

void Renderer::drawTutorial() {
    drawRect(0, 0, 1, 1, kBackground);
    drawText("HOW TO PLAY", 0.50f, 0.07f, 0.025f, kText);
    drawText("DRAG A PIECE TO THE BOARD", 0.50f, 0.20f, 0.010f, kAccent);
    drawText("FILL A ROW OR COLUMN", 0.50f, 0.29f, 0.010f, kText);
    drawText("CLEAR MORE FOR BIG BONUSES", 0.50f, 0.38f, 0.009f, kAccent);
    drawText("KEEP SPACE FOR LARGE PIECES", 0.50f, 0.47f, 0.009f, kText);
    drawText("EARN LUMEN AND RESTORE WORLDS", 0.50f, 0.56f, 0.009f, kAccent);
    drawText("WHITE PRISMS CAN OVERLAP", 0.50f, 0.65f, 0.009f, kText);
    drawText("ORANGE LANTERNS CLEAR NEARBY", 0.50f, 0.71f, 0.008f, kText);
    drawButton(0.20f, 0.83f, 0.80f, 0.90f, "PLAY", kAccent);
}

void Renderer::drawSettings() {
    drawRect(0, 0, 1, 1, kBackground);
    drawText("SETTINGS", 0.50f, 0.08f, 0.025f, kText);
    drawButton(0.14f, 0.24f, 0.86f, 0.31f,
               profile_.soundEnabled ? "SOUND ON" : "SOUND OFF", kPanel);
    drawButton(0.14f, 0.35f, 0.86f, 0.42f,
               profile_.hapticsEnabled ? "HAPTICS ON" : "HAPTICS OFF", kPanel);
    drawButton(0.14f, 0.46f, 0.86f, 0.53f,
               profile_.highContrast ? "CONTRAST ON" : "CONTRAST OFF", kPanel);
    drawButton(0.14f, 0.59f, 0.86f, 0.66f, "PRIVACY", kPanel);
    drawButton(0.20f, 0.82f, 0.80f, 0.89f, "BACK", kAccent);
}

void Renderer::drawStats() {
    drawRect(0, 0, 1, 1, kBackground);
    drawText("STATISTICS", 0.50f, 0.06f, 0.024f, kText);
    drawText("GAMES", 0.24f, 0.19f, 0.011f, kAccent);
    drawNumber(profile_.gamesPlayed, 0.72f, 0.18f, 0.026f, kText);
    drawText("PIECES", 0.24f, 0.29f, 0.011f, kAccent);
    drawNumber(profile_.totalPieces, 0.72f, 0.28f, 0.026f, kText);
    drawText("LINES", 0.24f, 0.39f, 0.011f, kAccent);
    drawNumber(profile_.totalLines, 0.72f, 0.38f, 0.026f, kText);
    drawText("MAX COMBO", 0.27f, 0.49f, 0.010f, kAccent);
    drawNumber(profile_.maxCombo, 0.72f, 0.48f, 0.026f, kText);
    drawText("TOP SCORES", 0.50f, 0.60f, 0.013f, kText);
    for (int i = 0; i < 5; ++i) {
        drawNumber(profile_.topScores[i], 0.50f, 0.65f + i * 0.045f, 0.018f,
                   i == 0 ? kAccent : kText);
    }
    drawButton(0.12f, 0.89f, 0.48f, 0.95f, "BACK", kAccent);
    drawButton(0.52f, 0.89f, 0.88f, 0.95f, "GLOBAL", kPanel);
}

void Renderer::drawAchievements() {
    drawRect(0, 0, 1, 1, kBackground);
    drawText("ACHIEVEMENTS", 0.50f, 0.055f, 0.021f, kText);
    struct Achievement { const char *name; bool earned; };
    const std::array<Achievement, 6> achievements{{
            {"FIRST LIGHT", profile_.totalLines >= 1},
            {"LINE KEEPER", profile_.totalLines >= 50},
            {"ARCHITECT", profile_.totalPieces >= 250},
            {"COMBO FIVE", profile_.maxCombo >= 5},
            {"BRIGHT WORLD", profile_.lumen >= 1000},
            {"MASTER", profile_.bestScores[0] >= 5000},
    }};
    for (size_t i = 0; i < achievements.size(); ++i) {
        Color card = achievements[i].earned ? Color{0.12f, 0.42f, 0.34f, 1} : kPanel;
        drawRect(0.10f, 0.16f + i * 0.105f, 0.90f, 0.235f + i * 0.105f, card);
        drawText(achievements[i].name, 0.50f, 0.184f + i * 0.105f, 0.011f,
                 achievements[i].earned ? kText : Color{0.45f, 0.50f, 0.60f, 1});
    }
    drawButton(0.12f, 0.86f, 0.48f, 0.93f, "BACK", kAccent);
    drawButton(0.52f, 0.86f, 0.88f, 0.93f, "GOOGLE", kPanel);
}

void Renderer::drawNumber(int value, float centerX, float top, float digitWidth,
                          const Color &color) {
    value = std::max(0, value);
    std::vector<int> digits;
    do {
        digits.push_back(value % 10);
        value /= 10;
    } while (value > 0);
    std::reverse(digits.begin(), digits.end());
    const float spacing = digitWidth * 0.24f;
    const float total = digits.size() * digitWidth + (digits.size() - 1) * spacing;
    float left = centerX - total * 0.5f;
    for (int digit : digits) {
        drawDigit(digit, left, top, digitWidth, color);
        left += digitWidth + spacing;
    }
}

void Renderer::drawDigit(int digit, float left, float top, float width, const Color &color) {
    static constexpr int masks[10] = {
            0x3f, 0x06, 0x5b, 0x4f, 0x66, 0x6d, 0x7d, 0x07, 0x7f, 0x6f};
    const float thickness = width * 0.18f;
    const float height = width * 1.75f;
    const int mask = masks[digit];
    auto horizontal = [&](float y) {
        drawRect(left + thickness, y, left + width - thickness, y + thickness, color);
    };
    auto vertical = [&](float x, float y) {
        drawRect(x, y + thickness, x + thickness, y + height * 0.5f - thickness, color);
    };
    if (mask & 0x01) horizontal(top);
    if (mask & 0x02) vertical(left + width - thickness, top);
    if (mask & 0x04) vertical(left + width - thickness, top + height * 0.5f);
    if (mask & 0x08) horizontal(top + height - thickness);
    if (mask & 0x10) vertical(left, top + height * 0.5f);
    if (mask & 0x20) vertical(left, top);
    if (mask & 0x40) horizontal(top + height * 0.5f - thickness * 0.5f);
}

void Renderer::drawText(const std::string &text, float centerX, float top, float size,
                        const Color &color) {
    const float pixel = size * 0.25f;
    const float glyphWidth = pixel * 5.0f;
    const float spacing = pixel * 1.4f;
    const float total = text.empty() ? 0.0f :
            text.size() * glyphWidth + (text.size() - 1) * spacing;
    float left = centerX - total * 0.5f;
    for (char glyph : text) {
        drawGlyph(static_cast<char>(std::toupper(glyph)), left, top, pixel, color);
        left += glyphWidth + spacing;
    }
}

void Renderer::drawGlyph(char glyph, float left, float top, float size, const Color &color) {
    const char *pattern = nullptr;
    switch (glyph) {
        case 'A': pattern="01110100011000111111100011000110001"; break;
        case 'B': pattern="11110100011000111110100011000111110"; break;
        case 'C': pattern="01111100001000010000100001000001111"; break;
        case 'D': pattern="11110100011000110001100011000111110"; break;
        case 'E': pattern="11111100001000011110100001000011111"; break;
        case 'F': pattern="11111100001000011110100001000010000"; break;
        case 'G': pattern="01111100001000010111100011000101111"; break;
        case 'H': pattern="10001100011000111111100011000110001"; break;
        case 'I': pattern="11111001000010000100001000010011111"; break;
        case 'J': pattern="00111000100001000010100100101001100"; break;
        case 'K': pattern="10001100101010011000101001001010001"; break;
        case 'L': pattern="10000100001000010000100001000011111"; break;
        case 'M': pattern="10001110111010110101100011000110001"; break;
        case 'N': pattern="10001110011010110011100011000110001"; break;
        case 'O': pattern="01110100011000110001100011000101110"; break;
        case 'P': pattern="11110100011000111110100001000010000"; break;
        case 'Q': pattern="01110100011000110001101011001001101"; break;
        case 'R': pattern="11110100011000111110101001001010001"; break;
        case 'S': pattern="01111100001000001110000010000111110"; break;
        case 'T': pattern="11111001000010000100001000010000100"; break;
        case 'U': pattern="10001100011000110001100011000101110"; break;
        case 'V': pattern="10001100011000110001100010101000100"; break;
        case 'W': pattern="10001100011000110101101011101110001"; break;
        case 'X': pattern="10001100010101000100010101000110001"; break;
        case 'Y': pattern="10001100010101000100001000010000100"; break;
        case 'Z': pattern="11111000010001000100010001000011111"; break;
        case '0': pattern="01110100011001110101110011000101110"; break;
        case '1': pattern="00100011000010000100001000010001110"; break;
        case '2': pattern="01110100010000100010001000100011111"; break;
        case '3': pattern="11110000010000101110000010000111110"; break;
        case '4': pattern="00010001100101010010111110001000010"; break;
        case '5': pattern="11111100001000011110000010000111110"; break;
        case '6': pattern="01110100001000011110100011000101110"; break;
        case '7': pattern="11111000010001000100010000100001000"; break;
        case '8': pattern="01110100011000101110100011000101110"; break;
        case '9': pattern="01110100011000101111000010000101110"; break;
        case '-': pattern="00000000000000011111000000000000000"; break;
        default: return;
    }
    const float pixelHeight = cellHeight(size);
    for (int row = 0; row < 7; ++row) {
        for (int column = 0; column < 5; ++column) {
            if (pattern[row * 5 + column] == '1') {
                drawRect(left + column * size, top + row * pixelHeight,
                         left + (column + 0.82f) * size,
                         top + (row + 0.82f) * pixelHeight, color);
            }
        }
    }
}

int Renderer::traySlotAt(float x, float y) const {
    if (y < kTrayTop - 0.04f || y > 1.0f) return -1;
    const int slot = std::clamp(static_cast<int>(x * 3.0f), 0, 2);
    return tray_[slot].used ? -1 : slot;
}

void Renderer::pointerDown(float x, float y) {
    if (screen_ == Screen::Home) {
        if (y > 0.30f && y < 0.38f) startGame(Mode::Classic);
        else if (y > 0.38f && y < 0.46f) startGame(Mode::Journey);
        else if (y > 0.46f && y < 0.54f && x < 0.50f) startGame(Mode::Daily);
        else if (y > 0.46f && y < 0.54f) startGame(Mode::Zen);
        else if (y > 0.54f && y < 0.62f) startGame(Mode::Rush);
        else if (y > 0.70f && y < 0.79f && x < 0.39f) screen_ = Screen::Tutorial;
        else if (y > 0.70f && y < 0.79f && x < 0.70f) screen_ = Screen::Stats;
        else if (y > 0.70f && y < 0.79f) screen_ = Screen::Settings;
        else if (y > 0.79f && y < 0.87f) screen_ = Screen::Achievements;
        return;
    }
    if (screen_ == Screen::Tutorial) {
        if (y > 0.80f) {
            profile_.tutorialSeen = true;
            saveProfile();
            screen_ = Screen::Home;
        }
        return;
    }
    if (screen_ == Screen::Settings) {
        if (y > 0.22f && y < 0.33f) profile_.soundEnabled = !profile_.soundEnabled;
        else if (y > 0.33f && y < 0.44f) profile_.hapticsEnabled = !profile_.hapticsEnabled;
        else if (y > 0.44f && y < 0.55f) profile_.highContrast = !profile_.highContrast;
        else if (y > 0.57f && y < 0.68f) openPrivacyPolicy();
        else if (y > 0.78f) screen_ = Screen::Home;
        saveProfile();
        return;
    }
    if (screen_ == Screen::Stats) {
        if (y > 0.84f && x < 0.50f) screen_ = Screen::Home;
        else if (y > 0.84f) showPlayGames(0);
        return;
    }
    if (screen_ == Screen::Achievements) {
        if (y > 0.82f && x < 0.50f) screen_ = Screen::Home;
        else if (y > 0.82f) showPlayGames(1);
        return;
    }
    if (screen_ != Screen::Playing) return;
    if (y > 0.12f && y < 0.19f && x < 0.16f) {
        saveProfile();
        screen_ = Screen::Home;
        return;
    }
    if (y > 0.12f && y < 0.19f && x > 0.84f) {
        paused_ = !paused_;
        return;
    }
    if (gameOver_) {
        if (y > 0.54f && y < 0.64f && x > 0.50f) shareScore();
        else if (y > 0.36f && y < 0.68f) startGame(mode_);
        return;
    }
    if (paused_) return;
    if (y > 0.68f && y < 0.77f && x > 0.68f && shufflesLeft_ > 0) {
        for (auto &piece : tray_) {
            if (!piece.used) piece = randomPiece();
        }
        --shufflesLeft_;
        sendFeedback(1);
        return;
    }
    draggedSlot_ = traySlotAt(x, y);
    touchX_ = x;
    touchY_ = y;
}

void Renderer::pointerMove(float x, float y) {
    if (draggedSlot_ < 0) return;
    touchX_ = x;
    touchY_ = y;
}

void Renderer::pointerUp(float x, float y) {
    if (draggedSlot_ < 0) return;
    touchX_ = x;
    touchY_ = y;
    placeDraggedPiece();
    draggedSlot_ = -1;
}

bool Renderer::dragPlacement(int &column, int &row) const {
    if (draggedSlot_ < 0) return false;
    const Piece &piece = tray_[draggedSlot_];
    const float pieceLeft = touchX_ - pieceWidth(piece) * kCell * 0.5f;
    const float pieceTop = touchY_ - 0.10f;
    column = static_cast<int>(std::round((pieceLeft - kBoardLeft) / kCell));
    row = static_cast<int>(std::round((pieceTop - kBoardTop) / cellHeight(kCell)));
    return true;
}

bool Renderer::canPlace(const Piece &piece, int column, int row) const {
    for (const auto &cell : piece.cells) {
        const int x = column + cell.x;
        const int y = row + cell.y;
        if (x < 0 || x >= kBoardSize || y < 0 || y >= kBoardSize ||
            (board_[y][x] != 0 && piece.special != 1)) {
            return false;
        }
    }
    return true;
}

void Renderer::placeDraggedPiece() {
    int column;
    int row;
    if (!dragPlacement(column, row)) return;
    Piece &piece = tray_[draggedSlot_];
    if (!canPlace(piece, column, row)) {
        sendFeedback(0);
        return;
    }
    for (const auto &cell : piece.cells) {
        board_[row + cell.y][column + cell.x] = piece.special == 1 ? 7 : piece.color;
    }
    if (piece.special == 2) {
        for (const auto &cell : piece.cells) {
            const int centerX = column + cell.x;
            const int centerY = row + cell.y;
            for (int offsetY = -1; offsetY <= 1; ++offsetY) {
                for (int offsetX = -1; offsetX <= 1; ++offsetX) {
                    const int x = centerX + offsetX;
                    const int y = centerY + offsetY;
                    if (x >= 0 && x < kBoardSize && y >= 0 && y < kBoardSize &&
                        !(offsetX == 0 && offsetY == 0)) board_[y][x] = 0;
                }
            }
        }
    }
    score_ += static_cast<int>(piece.cells.size()) * 10;
    ++piecesThisRun_;
    ++profile_.totalPieces;
    piece.used = true;
    clearCompletedLines();
    sendFeedback(lastClearCount_ > 0 ? 2 : 1);

    bool trayEmpty = true;
    for (const auto &candidate : tray_) trayEmpty &= candidate.used;
    if (trayEmpty) refillTray();
    bestScore_ = std::max(bestScore_, score_);
    profile_.bestScores[modeIndex()] = std::max(profile_.bestScores[modeIndex()], score_);
    if (mode_ == Mode::Journey && linesThisRun_ >= objectiveTarget()) {
        ++profile_.journeyStage;
        profile_.lumen += 100;
        feedbackTimer_ = 2.0f;
        finishGame();
    } else if (!hasAnyMove()) {
        if (mode_ == Mode::Zen) {
            refillTray();
            tray_[0] = {{{0, 0}}, 2, 0, false};
        } else {
            finishGame();
        }
    }
    saveProfile();
}

void Renderer::clearCompletedLines() {
    std::array<bool, kBoardSize> rows{};
    std::array<bool, kBoardSize> columns{};
    int lineCount = 0;
    for (int row = 0; row < kBoardSize; ++row) {
        rows[row] = true;
        for (int column = 0; column < kBoardSize; ++column) rows[row] &= board_[row][column] != 0;
        if (rows[row]) ++lineCount;
    }
    for (int column = 0; column < kBoardSize; ++column) {
        columns[column] = true;
        for (int row = 0; row < kBoardSize; ++row) columns[column] &= board_[row][column] != 0;
        if (columns[column]) ++lineCount;
    }
    if (lineCount == 0) {
        combo_ = 0;
        lastClearCount_ = 0;
        return;
    }
    for (int row = 0; row < kBoardSize; ++row) {
        for (int column = 0; column < kBoardSize; ++column) {
            if (rows[row] || columns[column]) board_[row][column] = 0;
        }
    }
    ++combo_;
    lastClearCount_ = lineCount;
    feedbackTimer_ = 1.2f;
    linesThisRun_ += lineCount;
    profile_.totalLines += lineCount;
    profile_.maxCombo = std::max(profile_.maxCombo, combo_);
    profile_.lumen += lineCount * 5 + std::max(0, combo_ - 1) * 2;
    score_ += lineCount * lineCount * 100 + (combo_ - 1) * 50;
}

bool Renderer::hasAnyMove() const {
    for (const auto &piece : tray_) {
        if (piece.used) continue;
        for (int row = 0; row < kBoardSize; ++row) {
            for (int column = 0; column < kBoardSize; ++column) {
                if (canPlace(piece, column, row)) return true;
            }
        }
    }
    return false;
}

bool Renderer::trayHasMove() const {
    return hasAnyMove();
}

void Renderer::handleInput() {
    android_input_buffer *buffer = android_app_swap_input_buffers(app_);
    if (!buffer) return;
    for (uint64_t i = 0; i < buffer->motionEventsCount; ++i) {
        GameActivityMotionEvent &event = buffer->motionEvents[i];
        const int action = event.action & AMOTION_EVENT_ACTION_MASK;
        const int pointerIndex = (event.action & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK) >>
                AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;
        if (action == AMOTION_EVENT_ACTION_DOWN) {
            const auto &pointer = event.pointers[pointerIndex];
            activePointerId_ = pointer.id;
            pointerDown(GameActivityPointerAxes_getX(&pointer) / width_,
                        GameActivityPointerAxes_getY(&pointer) / height_);
        } else if (action == AMOTION_EVENT_ACTION_MOVE && activePointerId_ >= 0) {
            for (uint32_t p = 0; p < event.pointerCount; ++p) {
                const auto &pointer = event.pointers[p];
                if (pointer.id == activePointerId_) {
                    pointerMove(GameActivityPointerAxes_getX(&pointer) / width_,
                                GameActivityPointerAxes_getY(&pointer) / height_);
                    break;
                }
            }
        } else if ((action == AMOTION_EVENT_ACTION_UP ||
                    action == AMOTION_EVENT_ACTION_CANCEL) && activePointerId_ >= 0) {
            const auto &pointer = event.pointers[pointerIndex];
            pointerUp(GameActivityPointerAxes_getX(&pointer) / width_,
                      GameActivityPointerAxes_getY(&pointer) / height_);
            activePointerId_ = -1;
        }
    }
    android_app_clear_motion_events(buffer);
    android_app_clear_key_events(buffer);
}
