#include "Renderer.h"

#include <android/input.h>
#include <algorithm>
#include <cassert>
#include <chrono>
#include <cmath>
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
    resetGame();
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

void Renderer::resetGame() {
    for (auto &row : board_) row.fill(0);
    score_ = 0;
    combo_ = 0;
    gameOver_ = false;
    draggedSlot_ = -1;
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
    return {shapes[shapeDistribution(random_)], colorDistribution(random_) + 1, false};
}

void Renderer::refillTray() {
    for (auto &piece : tray_) piece = randomPiece();
}

void Renderer::render() {
    updateRenderArea();
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
    const Color &color = kBlockColors[static_cast<size_t>(piece.color - 1)];
    for (const auto &cell : piece.cells) {
        drawCell(originX + cell.x * cellSize, originY + cell.y * cellHeight(cellSize),
                 cellSize, color, alpha);
    }
}

void Renderer::drawScene() {
    drawRect(0.0f, 0.0f, 1.0f, 1.0f, kBackground);

    // Original "restoration" identity: a horizon that brightens with the score.
    const float progress = std::min(1.0f, score_ / 1200.0f);
    Color glow{0.12f + progress * 0.20f, 0.12f + progress * 0.30f,
               0.25f + progress * 0.35f, 1.0f};
    drawRect(0.0f, 0.0f, 1.0f, 0.205f, glow);
    drawRect(0.0f, 0.18f, 1.0f, 0.205f, kAccent);

    drawNumber(score_, 0.50f, 0.055f, 0.055f, kText);
    drawNumber(bestScore_, 0.87f, 0.075f, 0.026f, kAccent);

    const float boardHeight = cellHeight(kBoardWidth);
    drawRect(kBoardLeft - 0.012f, kBoardTop - 0.008f,
             kBoardLeft + kBoardWidth + 0.012f,
             kBoardTop + boardHeight + 0.008f, kPanel);
    for (int row = 0; row < kBoardSize; ++row) {
        for (int column = 0; column < kBoardSize; ++column) {
            const float left = kBoardLeft + column * kCell;
            const float top = kBoardTop + row * cellHeight(kCell);
            if (board_[row][column] == 0) {
                drawCell(left, top, kCell, kEmptyCell);
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
        drawNumber(score_, 0.50f, 0.45f, 0.065f, kDanger);
        // A wide restart bar; tapping anywhere on the overlay starts a new run.
        drawRect(0.28f, 0.565f, 0.72f, 0.605f, kAccent);
    }
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

int Renderer::traySlotAt(float x, float y) const {
    if (y < kTrayTop - 0.04f || y > 1.0f) return -1;
    const int slot = std::clamp(static_cast<int>(x * 3.0f), 0, 2);
    return tray_[slot].used ? -1 : slot;
}

void Renderer::pointerDown(float x, float y) {
    if (gameOver_) {
        if (y > 0.36f && y < 0.68f) resetGame();
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
        if (x < 0 || x >= kBoardSize || y < 0 || y >= kBoardSize || board_[y][x] != 0) {
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
    if (!canPlace(piece, column, row)) return;
    for (const auto &cell : piece.cells) board_[row + cell.y][column + cell.x] = piece.color;
    score_ += static_cast<int>(piece.cells.size()) * 10;
    piece.used = true;
    clearCompletedLines();

    bool trayEmpty = true;
    for (const auto &candidate : tray_) trayEmpty &= candidate.used;
    if (trayEmpty) refillTray();
    bestScore_ = std::max(bestScore_, score_);
    gameOver_ = !hasAnyMove();
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
        return;
    }
    for (int row = 0; row < kBoardSize; ++row) {
        for (int column = 0; column < kBoardSize; ++column) {
            if (rows[row] || columns[column]) board_[row][column] = 0;
        }
    }
    ++combo_;
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
