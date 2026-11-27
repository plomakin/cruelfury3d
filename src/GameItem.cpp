#include "GameItem.h"
#include "TextureAtlas.h"

#include <GLES2/gl2.h>
#include <QCursor>
#include <QHoverEvent>
#include <QMatrix4x4>
#include <QMouseEvent>
#include <QOpenGLContext>
#include <QOpenGLFramebufferObject>
#include <QOpenGLFramebufferObjectFormat>
#include <QQuickWindow>
#include <QWheelEvent>
#include <algorithm>
#include <cmath>
#include <vector>

namespace {

struct Vertex3D {
    float x, y, z;
    float u, v;
    float r, g, b;
    float fullbright;
};

const char* kWorldVS = R"(
attribute vec3 a_pos;
attribute vec2 a_uv;
attribute vec3 a_tint;
attribute float a_fullbright;

uniform mat4 u_mvp;

varying vec3 v_worldPos;
varying vec2 v_uv;
varying vec3 v_tint;
varying float v_fullbright;

void main() {
    v_worldPos = a_pos;
    v_uv = a_uv;
    v_tint = a_tint;
    v_fullbright = a_fullbright;
    gl_Position = u_mvp * vec4(a_pos, 1.0);
}
)";

const char* kWorldFS = R"(
precision mediump float;

varying vec3 v_worldPos;
varying vec2 v_uv;
varying vec3 v_tint;
varying float v_fullbright;

uniform sampler2D u_tex;
uniform vec3 u_camPos;
uniform int u_numLights;
uniform vec4 u_lightPosRad[8];
uniform vec3 u_lightColor[8];
uniform float u_overdrive;

void main() {
    vec4 tex = texture2D(u_tex, v_uv);
    if (tex.a < 0.45) discard;

    float maxC = max(tex.r, max(tex.g, tex.b));
    float minC = min(tex.r, min(tex.g, tex.b));
    float sat = maxC - minC;
    float neonGlow = (sat > 0.42 && maxC > 0.72) ? 0.88 : 0.0;
    float fb = max(v_fullbright, neonGlow);

    vec3 lightAcc = vec3(0.38, 0.35, 0.48);
    for (int i = 0; i < 8; ++i) {
        if (i >= u_numLights) break;
        vec3 d = u_lightPosRad[i].xyz - v_worldPos;
        float dist = length(d);
        float rad = u_lightPosRad[i].w;
        float att = clamp(1.0 - (dist / rad), 0.0, 1.0);
        lightAcc += u_lightColor[i] * (att * att * 1.15);
    }

    vec3 lit = tex.rgb * v_tint * mix(lightAcc, vec3(1.18), fb);
    lit += vec3(0.18, 0.04, 0.16) * u_overdrive;

    float camDist = length(v_worldPos - u_camPos);
    float fog = clamp((camDist - 7.0) / 28.0, 0.0, 0.88);
    vec3 fogCol = vec3(0.05, 0.025, 0.10);
    gl_FragColor = vec4(mix(lit, fogCol, fog * (1.0 - fb * 0.45)), 1.0);
}
)";

const char* kOverlayVS = R"(
attribute vec2 a_pos2d;
attribute vec2 a_uv2d;
varying vec2 v_uv;
void main() {
    v_uv = a_uv2d;
    gl_Position = vec4(a_pos2d, 0.0, 1.0);
}
)";

const char* kOverlayFS = R"(
precision mediump float;
varying vec2 v_uv;
uniform sampler2D u_tex;
uniform vec4 u_color;
uniform float u_useTex;
void main() {
    if (u_useTex > 0.5) {
        vec4 t = texture2D(u_tex, v_uv);
        if (t.a < 0.35) discard;
        gl_FragColor = vec4(t.rgb * u_color.rgb, t.a * u_color.a);
    } else {
        gl_FragColor = u_color;
    }
}
)";

GLuint compileShaderProgram(const char* vsSrc, const char* fsSrc) {
    auto compileOne = [](GLenum type, const char* src) -> GLuint {
        GLuint s = glCreateShader(type);
        glShaderSource(s, 1, &src, nullptr);
        glCompileShader(s);
        return s;
    };
    GLuint vs = compileOne(GL_VERTEX_SHADER, vsSrc);
    GLuint fs = compileOne(GL_FRAGMENT_SHADER, fsSrc);
    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);
    glDeleteShader(vs);
    glDeleteShader(fs);
    return prog;
}

void pushQuad(std::vector<Vertex3D>& v,
              float x0, float y0, float z0,
              float x1, float y1, float z1,
              float x2, float y2, float z2,
              float x3, float y3, float z3,
              const AtlasUV& uv,
              float r, float g, float b, float fb = 0.0f) {
    v.push_back({x0, y0, z0, uv.u0, uv.v1, r, g, b, fb});
    v.push_back({x1, y1, z1, uv.u1, uv.v1, r, g, b, fb});
    v.push_back({x2, y2, z2, uv.u1, uv.v0, r, g, b, fb});

    v.push_back({x0, y0, z0, uv.u0, uv.v1, r, g, b, fb});
    v.push_back({x2, y2, z2, uv.u1, uv.v0, r, g, b, fb});
    v.push_back({x3, y3, z3, uv.u0, uv.v0, r, g, b, fb});
}

int wallTileForCell(uint8_t c) {
    if (c >= 1 && c <= 16) return c - 1;
    switch (c) {
        case 20: return TexTile::Door;
        case 21: return TexTile::LockedDoor;
        case 22: return TexTile::SecretWall;
        case 23: return TexTile::SodaMachine;
        case 24: return TexTile::SodaSmashed;
        case 25: return TexTile::ExitSwitch;
        case 26: return TexTile::Crate;
        default: return TexTile::SteelWall;
    }
}

class FpsRenderer : public QQuickFramebufferObject::Renderer {
public:
    ~FpsRenderer() override {
        if (m_texId) glDeleteTextures(1, &m_texId);
        if (m_worldProg) glDeleteProgram(m_worldProg);
        if (m_ovProg) glDeleteProgram(m_ovProg);
    }

    QOpenGLFramebufferObject* createFramebufferObject(const QSize& size) override {
        QOpenGLFramebufferObjectFormat fmt;
        fmt.setAttachment(QOpenGLFramebufferObject::CombinedDepthStencil);
        // Render at crisp high-FPS scale (cap max edge ~1080px for locked 60-120 FPS on Mali-G57)
        QSize target = size;
        const int maxDim = std::max(size.width(), size.height());
        if (maxDim > 1080) {
            float scale = 1080.0f / static_cast<float>(maxDim);
            target = QSize(std::max(64, int(size.width() * scale)),
                           std::max(64, int(size.height() * scale)));
        }
        m_fbSize = target;
        return new QOpenGLFramebufferObject(target, fmt);
    }

    void synchronize(QQuickFramebufferObject* item) override {
        auto* game = static_cast<GameItem*>(item);
        m_window = game->window();
        game->syncToSnapshot(m_snap);
    }

    void render() override {
        if (!QOpenGLContext::currentContext()) return;
        ensureGLResources();

        const int w = std::max(1, m_fbSize.width());
        const int h = std::max(1, m_fbSize.height());
        glViewport(0, 0, w, h);

        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
        glDisable(GL_CULL_FACE);
        glDisable(GL_SCISSOR_TEST);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LEQUAL);
        glDepthMask(GL_TRUE);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        glClearColor(0.05f, 0.025f, 0.10f, 1.0f);
        glClearDepthf(1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        if (m_cachedRevision != m_snap.mapRevision) {
            rebuildStaticMesh();
            m_cachedRevision = m_snap.mapRevision;
        }

        // Build camera projection * view matrix
        QMatrix4x4 proj;
        const float aspect = static_cast<float>(w) / static_cast<float>(h);
        proj.perspective(m_snap.fovDeg, aspect, 0.12f, 64.0f);

        QMatrix4x4 view;
        view.rotate(-m_snap.camRoll * 57.29578f, 0.0f, 0.0f, 1.0f);
        view.rotate(-m_snap.camPitch * 57.29578f, 1.0f, 0.0f, 0.0f);
        view.rotate(m_snap.camYaw * 57.29578f, 0.0f, 1.0f, 0.0f);
        view.translate(-m_snap.camX, -m_snap.camY, -m_snap.camZ);

        const QMatrix4x4 mvp = proj * view;

        // Assemble dynamic geometry (sliding doors + sorted billboards)
        m_dynVerts.clear();
        buildDynamicDoors(m_dynVerts);
        buildBillboards(m_dynVerts);

        glUseProgram(m_worldProg);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, m_texId);
        glUniform1i(m_uTex, 0);
        glUniformMatrix4fv(m_uMvp, 1, GL_FALSE, mvp.constData());
        glUniform3f(m_uCamPos, m_snap.camX, m_snap.camY, m_snap.camZ);
        glUniform1f(m_uOverdrive, m_snap.overdriveGlow);

        const int numLights = std::min(8, static_cast<int>(m_snap.lights.size()));
        float lightPosRad[32] = {};
        float lightCol[24] = {};
        for (int i = 0; i < numLights; ++i) {
            const auto& L = m_snap.lights[i];
            lightPosRad[i * 4 + 0] = L.x;
            lightPosRad[i * 4 + 1] = L.y;
            lightPosRad[i * 4 + 2] = L.z;
            lightPosRad[i * 4 + 3] = L.radius;
            lightCol[i * 3 + 0] = L.r;
            lightCol[i * 3 + 1] = L.g;
            lightCol[i * 3 + 2] = L.b;
        }
        glUniform1i(m_uNumLights, numLights);
        if (numLights > 0) {
            glUniform4fv(m_uLightPosRad, numLights, lightPosRad);
            glUniform3fv(m_uLightCol, numLights, lightCol);
        }

        drawVertexBatch(m_staticVerts);
        drawVertexBatch(m_dynVerts);

        // Draw First-Person Weapon Viewmodel, Muzzle Flash, Mighty Boot & Screen Tint!
        glDisable(GL_DEPTH_TEST);
        renderViewmodelsAndFX(aspect);

        if (m_window) {
            m_window->resetOpenGLState();
        }
        update(); // Continuous 60 Hz frame pacing
    }

private:
    void ensureGLResources() {
        if (m_worldProg != 0) return;

        m_worldProg = compileShaderProgram(kWorldVS, kWorldFS);
        m_aPos = glGetAttribLocation(m_worldProg, "a_pos");
        m_aUv = glGetAttribLocation(m_worldProg, "a_uv");
        m_aTint = glGetAttribLocation(m_worldProg, "a_tint");
        m_aFb = glGetAttribLocation(m_worldProg, "a_fullbright");
        m_uMvp = glGetUniformLocation(m_worldProg, "u_mvp");
        m_uTex = glGetUniformLocation(m_worldProg, "u_tex");
        m_uCamPos = glGetUniformLocation(m_worldProg, "u_camPos");
        m_uNumLights = glGetUniformLocation(m_worldProg, "u_numLights");
        m_uLightPosRad = glGetUniformLocation(m_worldProg, "u_lightPosRad");
        m_uLightCol = glGetUniformLocation(m_worldProg, "u_lightColor");
        m_uOverdrive = glGetUniformLocation(m_worldProg, "u_overdrive");

        m_ovProg = compileShaderProgram(kOverlayVS, kOverlayFS);
        m_aPos2d = glGetAttribLocation(m_ovProg, "a_pos2d");
        m_aUv2d = glGetAttribLocation(m_ovProg, "a_uv2d");
        m_uOvTex = glGetUniformLocation(m_ovProg, "u_tex");
        m_uOvColor = glGetUniformLocation(m_ovProg, "u_color");
        m_uOvUseTex = glGetUniformLocation(m_ovProg, "u_useTex");

        const auto rgba = TextureAtlas::generateRGBA();
        glGenTextures(1, &m_texId);
        glBindTexture(GL_TEXTURE_2D, m_texId);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
                     TextureAtlas::kAtlasDim, TextureAtlas::kAtlasDim, 0,
                     GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }

    void drawVertexBatch(const std::vector<Vertex3D>& verts) {
        if (verts.empty()) return;
        const char* base = reinterpret_cast<const char*>(verts.data());
        const GLsizei stride = sizeof(Vertex3D);
        glEnableVertexAttribArray(m_aPos);
        glEnableVertexAttribArray(m_aUv);
        glEnableVertexAttribArray(m_aTint);
        glEnableVertexAttribArray(m_aFb);
        glVertexAttribPointer(m_aPos, 3, GL_FLOAT, GL_FALSE, stride, base + 0);
        glVertexAttribPointer(m_aUv, 2, GL_FLOAT, GL_FALSE, stride, base + 3 * sizeof(float));
        glVertexAttribPointer(m_aTint, 3, GL_FLOAT, GL_FALSE, stride, base + 5 * sizeof(float));
        glVertexAttribPointer(m_aFb, 1, GL_FLOAT, GL_FALSE, stride, base + 8 * sizeof(float));
        glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(verts.size()));
        glDisableVertexAttribArray(m_aPos);
        glDisableVertexAttribArray(m_aUv);
        glDisableVertexAttribArray(m_aTint);
        glDisableVertexAttribArray(m_aFb);
    }

    void rebuildStaticMesh() {
        m_staticVerts.clear();
        if (m_snap.cells.empty()) return;
        const int W = m_snap.mapW;
        const int H = m_snap.mapH;
        const float cs = GameSim::kCell;
        const float ch = m_snap.ceilHeight;
        const AtlasUV floorUV = TextureAtlas::tileUV(m_snap.floorTile);
        const AtlasUV ceilUV = TextureAtlas::tileUV(m_snap.ceilTile);

        auto cellAt = [&](int x, int z) -> uint8_t {
            if (x < 0 || z < 0 || x >= W || z >= H) return 1;
            return m_snap.cells[z * W + x];
        };
        auto isStaticBlock = [](uint8_t c) {
            return c != 0 && c != 20 && c != 21;
        };

        for (int z = 0; z < H; ++z) {
            for (int x = 0; x < W; ++x) {
                const float x0 = x * cs, x1 = (x + 1) * cs;
                const float z0 = z * cs, z1 = (z + 1) * cs;
                const uint8_t c = cellAt(x, z);

                // Draw floor & ceiling everywhere except solid non-crate blocks
                if (!isStaticBlock(c) || c == 26) {
                    pushQuad(m_staticVerts,
                             x0, 0.0f, z1, x1, 0.0f, z1, x1, 0.0f, z0, x0, 0.0f, z0,
                             floorUV, 0.92f, 0.92f, 0.98f);
                    pushQuad(m_staticVerts,
                             x0, ch, z0, x1, ch, z0, x1, ch, z1, x0, ch, z1,
                             ceilUV, 0.85f, 0.85f, 0.95f);
                }

                if (!isStaticBlock(c)) continue;

                const float topY = (c == 26) ? 1.35f : ch; // Crates are half-height 3D blocks!
                const AtlasUV wuv = TextureAtlas::tileUV(wallTileForCell(c));
                const float fb = (c == 23 || c == 25) ? 0.55f : 0.0f;

                if (c == 26) {
                    // Crate top face
                    pushQuad(m_staticVerts,
                             x0 + 0.2f, topY, z1 - 0.2f, x1 - 0.2f, topY, z1 - 0.2f,
                             x1 - 0.2f, topY, z0 + 0.2f, x0 + 0.2f, topY, z0 + 0.2f,
                             wuv, 1.0f, 0.95f, 0.9f);
                }

                const float pad = (c == 26) ? 0.2f : 0.0f;
                // North (-Z), South (+Z), West (-X), East (+X) faces
                if (!isStaticBlock(cellAt(x, z - 1)) || c == 26) {
                    pushQuad(m_staticVerts,
                             x1 - pad, 0.0f, z0 + pad, x0 + pad, 0.0f, z0 + pad,
                             x0 + pad, topY, z0 + pad, x1 - pad, topY, z0 + pad,
                             wuv, 1.0f, 1.0f, 1.0f, fb);
                }
                if (!isStaticBlock(cellAt(x, z + 1)) || c == 26) {
                    pushQuad(m_staticVerts,
                             x0 + pad, 0.0f, z1 - pad, x1 - pad, 0.0f, z1 - pad,
                             x1 - pad, topY, z1 - pad, x0 + pad, topY, z1 - pad,
                             wuv, 1.0f, 1.0f, 1.0f, fb);
                }
                if (!isStaticBlock(cellAt(x - 1, z)) || c == 26) {
                    pushQuad(m_staticVerts,
                             x0 + pad, 0.0f, z0 + pad, x0 + pad, 0.0f, z1 - pad,
                             x0 + pad, topY, z1 - pad, x0 + pad, topY, z0 + pad,
                             wuv, 0.80f, 0.82f, 0.90f, fb);
                }
                if (!isStaticBlock(cellAt(x + 1, z)) || c == 26) {
                    pushQuad(m_staticVerts,
                             x1 - pad, 0.0f, z1 - pad, x1 - pad, 0.0f, z0 + pad,
                             x1 - pad, topY, z0 + pad, x1 - pad, topY, z1 - pad,
                             wuv, 0.80f, 0.82f, 0.90f, fb);
                }
            }
        }
    }

    void buildDynamicDoors(std::vector<Vertex3D>& out) {
        if (m_snap.cells.empty()) return;
        const int W = m_snap.mapW;
        const int H = m_snap.mapH;
        const float cs = GameSim::kCell;
        const float ch = m_snap.ceilHeight;

        for (int z = 0; z < H; ++z) {
            for (int x = 0; x < W; ++x) {
                const int idx = z * W + x;
                const uint8_t c = m_snap.cells[idx];
                if (c != 20 && c != 21) continue;
                const float openAmt = (idx < static_cast<int>(m_snap.doorOpen.size())) ? m_snap.doorOpen[idx] : 0.0f;
                if (openAmt >= 0.98f) continue;

                const float y0 = openAmt * (ch * 0.92f);
                const float y1 = y0 + ch;
                const float x0 = x * cs, x1 = (x + 1) * cs;
                const float z0 = z * cs, z1 = (z + 1) * cs;
                const AtlasUV uv = TextureAtlas::tileUV(wallTileForCell(c));

                pushQuad(out, x1, y0, z0 + 0.35f, x0, y0, z0 + 0.35f, x0, y1, z0 + 0.35f, x1, y1, z0 + 0.35f, uv, 1.0f, 1.0f, 1.0f, 0.2f);
                pushQuad(out, x0, y0, z1 - 0.35f, x1, y0, z1 - 0.35f, x1, y1, z1 - 0.35f, x0, y1, z1 - 0.35f, uv, 1.0f, 1.0f, 1.0f, 0.2f);
                pushQuad(out, x0 + 0.35f, y0, z0, x0 + 0.35f, y0, z1, x0 + 0.35f, y1, z1, x0 + 0.35f, y1, z0, uv, 0.85f, 0.85f, 0.95f, 0.2f);
                pushQuad(out, x1 - 0.35f, y0, z1, x1 - 0.35f, y0, z0, x1 - 0.35f, y1, z0, x1 - 0.35f, y1, z1, uv, 0.85f, 0.85f, 0.95f, 0.2f);
            }
        }
    }

    void buildBillboards(std::vector<Vertex3D>& out) {
        std::vector<RenderBillboard> sorted = m_snap.billboards;
        std::sort(sorted.begin(), sorted.end(), [&](const RenderBillboard& a, const RenderBillboard& b) {
            float da = (a.x - m_snap.camX) * (a.x - m_snap.camX) + (a.z - m_snap.camZ) * (a.z - m_snap.camZ);
            float db = (b.x - m_snap.camX) * (b.x - m_snap.camX) + (b.z - m_snap.camZ) * (b.z - m_snap.camZ);
            return da > db;
        });

        const float rx = std::cos(m_snap.camYaw);
        const float rz = std::sin(m_snap.camYaw);

        for (const auto& b : sorted) {
            const float hw = b.width * 0.5f;
            const float hh = b.height * 0.5f;
            const AtlasUV uv = TextureAtlas::tileUV(b.tile);
            pushQuad(out,
                     b.x - rx * hw, b.y - hh, b.z - rz * hw,
                     b.x + rx * hw, b.y - hh, b.z + rz * hw,
                     b.x + rx * hw, b.y + hh, b.z + rz * hw,
                     b.x - rx * hw, b.y + hh, b.z - rz * hw,
                     uv, b.tintR, b.tintG, b.tintB, b.fullbright ? 1.0f : 0.0f);
        }
    }

    void drawOverlayQuad(float x0, float y0, float x1, float y1, const AtlasUV* uv, float r, float g, float b, float a) {
        const float u0 = uv ? uv->u0 : 0.0f;
        const float v0 = uv ? uv->v0 : 0.0f;
        const float u1 = uv ? uv->u1 : 1.0f;
        const float v1 = uv ? uv->v1 : 1.0f;
        const float verts[24] = {
            x0, y0, u0, v1,
            x1, y0, u1, v1,
            x1, y1, u1, v0,
            x0, y0, u0, v1,
            x1, y1, u1, v0,
            x0, y1, u0, v0
        };
        glUniform4f(m_uOvColor, r, g, b, a);
        glUniform1f(m_uOvUseTex, uv ? 1.0f : 0.0f);
        glEnableVertexAttribArray(m_aPos2d);
        glEnableVertexAttribArray(m_aUv2d);
        glVertexAttribPointer(m_aPos2d, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), verts);
        glVertexAttribPointer(m_aUv2d, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), verts + 2);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glDisableVertexAttribArray(m_aPos2d);
        glDisableVertexAttribArray(m_aUv2d);
    }

    void renderViewmodelsAndFX(float aspect) {
        glUseProgram(m_ovProg);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, m_texId);
        glUniform1i(m_uOvTex, 0);

        const float wScale = 1.0f / std::max(0.65f, std::min(1.65f, aspect));

        // 1) Muzzle flash starburst behind weapon tip
        if (m_snap.muzzleTimer > 0.0f) {
            AtlasUV muv = TextureAtlas::tileUV(TexTile::MuzzleFlash);
            float cx = 0.22f + m_snap.weaponBobX;
            float cy = -0.32f - m_snap.weaponRecoil * 0.06f;
            float rad = 0.28f;
            drawOverlayQuad(cx - rad * wScale, cy - rad, cx + rad * wScale, cy + rad, &muv, 1.0f, 0.95f, 0.75f, 1.0f);
        }

        // 2) Mighty Boot thrusting across center-left!
        if (m_snap.bootTimer > 0.0f) {
            float prog = std::sin((m_snap.bootTimer / 0.30f) * 3.14159f);
            AtlasUV buv = TextureAtlas::tileUV(TexTile::MightyBoot);
            float bx = -0.45f + prog * 0.42f;
            float by = -1.05f + prog * 0.72f;
            drawOverlayQuad(bx, by, bx + 0.85f * wScale, by + 0.95f, &buv, 1.0f, 1.0f, 1.0f, 1.0f);
        }

        // 3) Current First-Person Weapon Sprite (with bob + kickback recoil!)
        int wepTile = TexTile::WepRevolver + std::clamp(m_snap.currentWeapon, 0, 3);
        AtlasUV wuv = TextureAtlas::tileUV(wepTile);
        float wx = 0.05f + m_snap.weaponBobX;
        float wy = -1.04f - m_snap.weaponBobY - m_snap.weaponRecoil * 0.14f;
        float ww = 0.82f * wScale;
        float wh = 0.86f;
        float lit = (m_snap.muzzleTimer > 0.0f) ? 1.35f : 1.0f;
        drawOverlayQuad(wx, wy, wx + ww, wy + wh, &wuv, lit, lit, lit, 1.0f);

        // 4) Hurt / Pickup / Overdrive fullscreen tint flash
        if (m_snap.hurtFlash > 0.01f) {
            drawOverlayQuad(-1.0f, -1.0f, 1.0f, 1.0f, nullptr, 0.95f, 0.08f, 0.12f, m_snap.hurtFlash * 0.45f);
        } else if (m_snap.pickupFlash > 0.01f) {
            drawOverlayQuad(-1.0f, -1.0f, 1.0f, 1.0f, nullptr, 0.15f, 0.92f, 1.0f, m_snap.pickupFlash * 0.32f);
        }
    }

    QQuickWindow* m_window = nullptr;
    QSize m_fbSize{640, 480};
    RenderSnapshot m_snap;
    uint32_t m_cachedRevision = 0;
    std::vector<Vertex3D> m_staticVerts;
    std::vector<Vertex3D> m_dynVerts;

    GLuint m_texId = 0;
    GLuint m_worldProg = 0;
    GLint m_aPos = -1, m_aUv = -1, m_aTint = -1, m_aFb = -1;
    GLint m_uMvp = -1, m_uTex = -1, m_uCamPos = -1;
    GLint m_uNumLights = -1, m_uLightPosRad = -1, m_uLightCol = -1, m_uOverdrive = -1;

    GLuint m_ovProg = 0;
    GLint m_aPos2d = -1, m_aUv2d = -1, m_uOvTex = -1, m_uOvColor = -1, m_uOvUseTex = -1;
};

} // namespace

GameItem::GameItem(QQuickItem* parent)
    : QQuickFramebufferObject(parent),
      m_audio(this),
      m_sim(&m_audio) {
    setMirrorVertically(true);
    setAcceptedMouseButtons(Qt::AllButtons);
    setAcceptHoverEvents(true);

    m_clock.start();
    m_lastNs = m_clock.nsecsElapsed();

    connect(&m_timer, &QTimer::timeout, this, &GameItem::onTick);
    m_timer.start(16); // ~60 Hz simulation loop
}

QQuickFramebufferObject::Renderer* GameItem::createRenderer() const {
    return new FpsRenderer();
}

void GameItem::startGame(int mode, int diff) {
    m_gameMode = std::clamp(mode, 0, 2);
    m_difficulty = std::clamp(diff, 0, 2);
    m_sim.startNewGame(static_cast<GameMode>(m_gameMode), m_difficulty);
    clearAllInputs();
    setPaused(false);
    emit gameModeChanged();
    emit difficultyChanged();
    emit hudChanged();
    update();
}

void GameItem::restartGame() {
    startGame(m_gameMode, m_difficulty);
}

void GameItem::setPaused(bool p) {
    if (m_paused == p) return;
    m_paused = p;
    m_audio.setPaused(p);
    if (!p) {
        m_lastNs = m_clock.nsecsElapsed();
    } else {
        clearAllInputs();
    }
    updateCursorVisibility();
    emit pausedChanged();
}

void GameItem::setGameMode(int m) {
    if (m_gameMode == m) return;
    m_gameMode = std::clamp(m, 0, 2);
    emit gameModeChanged();
}

void GameItem::setDifficulty(int d) {
    if (m_difficulty == d) return;
    m_difficulty = std::clamp(d, 0, 2);
    emit difficultyChanged();
}

void GameItem::setLookSensitivity(float s) {
    m_lookSensitivity = std::clamp(s, 0.25f, 3.0f);
    emit settingsChanged();
}

void GameItem::setGyroEnabled(bool on) {
    m_gyroEnabled = on;
    emit settingsChanged();
}

void GameItem::setHapticsEnabled(bool on) {
    m_hapticsEnabled = on;
    emit settingsChanged();
}

void GameItem::setMusicEnabled(bool on) {
    m_musicEnabled = on;
    m_audio.setMusicEnabled(on);
    emit settingsChanged();
}

void GameItem::setSfxEnabled(bool on) {
    m_sfxEnabled = on;
    m_audio.setSfxEnabled(on);
    emit settingsChanged();
}

void GameItem::setAutoFire(bool on) {
    m_autoFire = on;
    emit settingsChanged();
}

void GameItem::setMoveInput(float forward, float strafe) {
    m_touchForward = std::clamp(forward, -1.0f, 1.0f);
    m_touchStrafe = std::clamp(strafe, -1.0f, 1.0f);
}

void GameItem::addTouchLook(float dxPixels, float dyPixels) {
    if (m_paused) return;
    const float radPerPx = 0.0046f * m_lookSensitivity;
    m_sim.addLookDelta(dxPixels * radPerPx, -dyPixels * radPerPx * 0.78f);
}

void GameItem::addGyroRates(float rxDegPerSec, float ryDegPerSec, float dtSec) {
    if (m_paused || !m_gyroEnabled) return;
    constexpr float kDegToRad = 0.0174532925f;
    m_sim.addLookDelta(-ryDegPerSec * kDegToRad * dtSec * 0.85f * m_lookSensitivity,
                       rxDegPerSec * kDegToRad * dtSec * 0.65f * m_lookSensitivity);
}

void GameItem::setFireHeld(bool held) {
    m_touchFireHeld = held;
}

void GameItem::altFire() {
    if (m_paused) return;
    m_sim.triggerAltFire();
    if (m_hapticsEnabled) emit hapticPulse(false);
    emit hudChanged();
}

void GameItem::bootKick() {
    if (m_paused) return;
    m_sim.triggerBootKick();
    if (m_hapticsEnabled) emit hapticPulse(true);
    emit hudChanged();
}

void GameItem::jumpOrDash() {
    if (m_paused) return;
    float fwd = 0.0f;
    float str = 0.0f;
    combinedMoveAxes(fwd, str);
    m_sim.triggerJumpOrDash(fwd, str);
    if (m_hapticsEnabled) emit hapticPulse(false);
}

void GameItem::interact() {
    if (m_paused) return;
    m_sim.triggerInteract();
    emit hudChanged();
}

void GameItem::selectWeapon(int index) {
    m_sim.selectWeapon(index);
    emit hudChanged();
}

void GameItem::cycleWeapon(int dir) {
    m_sim.cycleWeapon(dir);
    emit hudChanged();
}

void GameItem::handleKeyEvent(int key, bool pressed) {
    bool isDesktopKey = true;
    switch (key) {
        case Qt::Key_W:
        case Qt::Key_Up:
            m_keyForward = pressed;
            break;
        case Qt::Key_S:
        case Qt::Key_Down:
            m_keyBack = pressed;
            break;
        case Qt::Key_A:
            m_keyStrafeLeft = pressed;
            break;
        case Qt::Key_D:
            m_keyStrafeRight = pressed;
            break;
        case Qt::Key_Left:
            m_keyTurnLeft = pressed;
            break;
        case Qt::Key_Right:
            m_keyTurnRight = pressed;
            break;
        case Qt::Key_Alt:
            m_keyAltStrafe = pressed;
            break;
        case Qt::Key_Control:
        case Qt::Key_Return:
        case Qt::Key_Enter:
            m_keyFire = pressed;
            break;
        case Qt::Key_Space:
        case Qt::Key_Shift:
            if (pressed) jumpOrDash();
            break;
        case Qt::Key_Q:
        case Qt::Key_F:
            if (pressed) bootKick();
            break;
        case Qt::Key_E:
            if (pressed) interact();
            break;
        case Qt::Key_R:
            if (pressed) altFire();
            break;
        case Qt::Key_1:
            if (pressed) selectWeapon(0);
            break;
        case Qt::Key_2:
            if (pressed) selectWeapon(1);
            break;
        case Qt::Key_3:
            if (pressed) selectWeapon(2);
            break;
        case Qt::Key_4:
            if (pressed) selectWeapon(3);
            break;
        case Qt::Key_BracketLeft:
            if (pressed) cycleWeapon(-1);
            break;
        case Qt::Key_BracketRight:
            if (pressed) cycleWeapon(1);
            break;
        default:
            isDesktopKey = false;
            break;
    }

    if (pressed && isDesktopKey && isGameplayInputActive()) {
        setDesktopInputActive(true);
    }
}

void GameItem::clearAllInputs() {
    m_touchForward = 0.0f;
    m_touchStrafe = 0.0f;
    m_touchFireHeld = false;
    m_keyForward = false;
    m_keyBack = false;
    m_keyStrafeLeft = false;
    m_keyStrafeRight = false;
    m_keyTurnLeft = false;
    m_keyTurnRight = false;
    m_keyAltStrafe = false;
    m_keyFire = false;
    m_mouseFireHeld = false;
    m_hasLastMousePos = false;
    m_expectingWarp = false;
    m_warpProbePending = false;
    m_edgeTurnRate = 0.0f;
}

void GameItem::notifyTouchActive() {
    m_hasLastMousePos = false;
    m_edgeTurnRate = 0.0f;
    setDesktopInputActive(false);
    updateCursorVisibility();
}

bool GameItem::isGameplayInputActive() const {
    return !m_paused && !m_sim.isGameOver() && !m_sim.isVictory();
}

void GameItem::setDesktopInputActive(bool active) {
    if (m_desktopInputActive == active) return;
    m_desktopInputActive = active;
    updateCursorVisibility();
    emit desktopInputActiveChanged();
}

void GameItem::updateCursorVisibility() {
    if (m_desktopInputActive && isGameplayInputActive()) {
        setCursor(Qt::BlankCursor);
    } else {
        unsetCursor();
    }
}

void GameItem::processMouseLook(const QPointF& localPos) {
    if (!isGameplayInputActive()) {
        m_hasLastMousePos = false;
        m_edgeTurnRate = 0.0f;
        updateCursorVisibility();
        return;
    }

    setDesktopInputActive(true);
    updateCursorVisibility();

    const float w = std::max(1.0f, static_cast<float>(width()));
    const float h = std::max(1.0f, static_cast<float>(height()));
    const QPointF center(w * 0.5f, h * 0.5f);

    if (m_warpProbePending) {
        m_warpProbePending = false;
        const bool landedNearCenter = (std::abs(localPos.x() - center.x()) < 10.0 &&
                                       std::abs(localPos.y() - center.y()) < 10.0);
        m_warpSupported = landedNearCenter;
        m_lastMousePos = localPos;
        m_hasLastMousePos = true;
        return;
    }

    if (m_expectingWarp) {
        m_expectingWarp = false;
        const bool nearCenter = (std::abs(localPos.x() - center.x()) < 12.0 &&
                                 std::abs(localPos.y() - center.y()) < 12.0);
        if (nearCenter) {
            m_lastMousePos = localPos;
            m_hasLastMousePos = true;
            return;
        }
    }

    if (!m_hasLastMousePos) {
        m_lastMousePos = localPos;
        m_hasLastMousePos = true;
        return;
    }

    const float dx = static_cast<float>(localPos.x() - m_lastMousePos.x());
    const float dy = static_cast<float>(localPos.y() - m_lastMousePos.y());
    m_lastMousePos = localPos;

    if (std::abs(dx) < 180.0f && std::abs(dy) < 180.0f) {
        const float radPerPx = 0.0036f * m_lookSensitivity;
        m_sim.addLookDelta(dx * radPerPx, -dy * radPerPx * 0.80f);
    }

    // Attempt pointer recentering on X11 / warp-capable compositors; otherwise use smooth edge turn
    const float edgeDistX = std::abs(static_cast<float>(localPos.x()) - center.x()) / (w * 0.5f);
    const float edgeDistY = std::abs(static_cast<float>(localPos.y()) - center.y()) / (h * 0.5f);
    if (!m_warpTested && (edgeDistX > 0.25f || edgeDistY > 0.25f)) {
        m_warpTested = true;
        m_warpProbePending = true;
        QCursor::setPos(mapToGlobal(center).toPoint());
        m_edgeTurnRate = 0.0f;
        return;
    }

    if (m_warpSupported) {
        m_edgeTurnRate = 0.0f;
        if (edgeDistX > 0.35f || edgeDistY > 0.35f) {
            m_expectingWarp = true;
            QCursor::setPos(mapToGlobal(center).toPoint());
        }
    } else {
        // Fallback for Wayland/Mir compositors that disallow cursor warping: smooth 12% edge turn
        const float normX = static_cast<float>(localPos.x()) / w;
        if (normX < 0.12f) {
            m_edgeTurnRate = -((0.12f - normX) / 0.12f);
        } else if (normX > 0.88f) {
            m_edgeTurnRate = ((normX - 0.88f) / 0.12f);
        } else {
            m_edgeTurnRate = 0.0f;
        }
    }
}

void GameItem::combinedMoveAxes(float& outForward, float& outStrafe) const {
    float keyFwd = (m_keyForward ? 1.0f : 0.0f) - (m_keyBack ? 1.0f : 0.0f);
    float keyStr = (m_keyStrafeRight ? 1.0f : 0.0f) - (m_keyStrafeLeft ? 1.0f : 0.0f);
    if (m_keyAltStrafe) {
        keyStr += (m_keyTurnRight ? 1.0f : 0.0f) - (m_keyTurnLeft ? 1.0f : 0.0f);
    }
    const float keyLen = std::hypot(keyFwd, keyStr);
    if (keyLen > 1.0f) {
        keyFwd /= keyLen;
        keyStr /= keyLen;
    }
    outForward = std::clamp(keyFwd + m_touchForward, -1.0f, 1.0f);
    outStrafe = std::clamp(keyStr + m_touchStrafe, -1.0f, 1.0f);
}

void GameItem::hoverEnterEvent(QHoverEvent* event) {
    if (isGameplayInputActive()) {
        setDesktopInputActive(true);
        m_lastMousePos = event->posF();
        m_hasLastMousePos = true;
    }
    event->accept();
}

void GameItem::hoverMoveEvent(QHoverEvent* event) {
    processMouseLook(event->posF());
    event->accept();
}

void GameItem::hoverLeaveEvent(QHoverEvent* event) {
    m_hasLastMousePos = false;
    m_edgeTurnRate = 0.0f;
    unsetCursor();
    event->accept();
}

void GameItem::mousePressEvent(QMouseEvent* event) {
    if (event->source() != Qt::MouseEventNotSynthesized || !isGameplayInputActive()) {
        event->ignore();
        return;
    }
    setDesktopInputActive(true);
    if (parentItem()) {
        parentItem()->forceActiveFocus(Qt::MouseFocusReason);
    }
    if (!m_hasLastMousePos) {
        m_lastMousePos = event->localPos();
        m_hasLastMousePos = true;
    }

    if (event->button() == Qt::LeftButton) {
        m_mouseFireHeld = true;
    } else if (event->button() == Qt::RightButton) {
        altFire();
    } else if (event->button() == Qt::MiddleButton) {
        bootKick();
    }
    event->accept();
}

void GameItem::mouseMoveEvent(QMouseEvent* event) {
    if (event->source() != Qt::MouseEventNotSynthesized || !isGameplayInputActive()) {
        event->ignore();
        return;
    }
    processMouseLook(event->localPos());
    event->accept();
}

void GameItem::mouseReleaseEvent(QMouseEvent* event) {
    if (event->source() != Qt::MouseEventNotSynthesized) {
        event->ignore();
        return;
    }
    if (event->button() == Qt::LeftButton) {
        m_mouseFireHeld = false;
    }
    event->accept();
}

void GameItem::wheelEvent(QWheelEvent* event) {
    if (!isGameplayInputActive()) {
        event->ignore();
        return;
    }
    const int dy = event->angleDelta().y();
    if (dy > 0) {
        cycleWeapon(1);
    } else if (dy < 0) {
        cycleWeapon(-1);
    }
    event->accept();
}

void GameItem::focusOutEvent(QFocusEvent* event) {
    clearAllInputs();
    updateCursorVisibility();
    QQuickFramebufferObject::focusOutEvent(event);
}

void GameItem::syncToSnapshot(RenderSnapshot& dst) const {
    m_sim.buildSnapshot(dst);
}

void GameItem::onTick() {
    const qint64 nowNs = m_clock.nsecsElapsed();
    const float dt = std::clamp(static_cast<float>(nowNs - m_lastNs) * 1e-9f, 0.001f, 0.05f);
    m_lastNs = nowNs;

    if (!m_paused) {
        if (isGameplayInputActive()) {
            const float keyTurn = m_keyAltStrafe
                                      ? 0.0f
                                      : ((m_keyTurnRight ? 1.0f : 0.0f) - (m_keyTurnLeft ? 1.0f : 0.0f));
            const float turnYaw = (keyTurn * 2.55f + m_edgeTurnRate * 2.25f) * m_lookSensitivity * dt;
            if (std::abs(turnYaw) > 1e-5f) {
                m_sim.addLookDelta(turnYaw, 0.0f);
            }
        }

        float moveForward = 0.0f;
        float moveStrafe = 0.0f;
        combinedMoveAxes(moveForward, moveStrafe);
        const bool fireHeld = m_touchFireHeld || m_mouseFireHeld || m_keyFire;

        SimEvents ev = m_sim.update(dt, moveForward, moveStrafe, fireHeld, m_autoFire);
        if (m_hapticsEnabled) {
            if (ev.triggerHapticHeavy) emit hapticPulse(true);
            else if (ev.triggerHapticLight) emit hapticPulse(false);
        }
        if (m_sim.isGameOver() || m_sim.isVictory()) {
            m_audio.setPaused(true);
            clearAllInputs();
            updateCursorVisibility();
        }
        emit hudChanged();
        update();
    }
}
