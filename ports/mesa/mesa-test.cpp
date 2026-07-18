#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#include <time.h>
#include <string_view>
#include <span>
#include <vector>

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GL/gl.h>
#include <GL/glext.h>

import nt;
import unique_handle;
import print;
import evdev;
import moxnox_ioctls;

using namespace std;

static span<uint32_t> framebuffer;
static unsigned int fb_width, fb_height, fb_pixels_per_scan_line;

// core GL 1.x functions have no PFN typedefs in glext.h
using PFN_glGetString = const GLubyte* (*)(GLenum);
using PFN_glViewport = void (*)(GLint, GLint, GLsizei, GLsizei);
using PFN_glClearColor = void (*)(GLfloat, GLfloat, GLfloat, GLfloat);
using PFN_glClear = void (*)(GLbitfield);
using PFN_glDrawArrays = void (*)(GLenum, GLint, GLsizei);
using PFN_glFinish = void (*)();
using PFN_glReadPixels = void (*)(GLint, GLint, GLsizei, GLsizei, GLenum,
                                  GLenum, void*);
using PFN_glPixelStorei = void (*)(GLenum, GLint);
using PFN_glGetError = GLenum (*)();
using PFN_glBegin = void (*)(GLenum);
using PFN_glEnd = void (*)();
using PFN_glColor3f = void (*)(GLfloat, GLfloat, GLfloat);
using PFN_glVertex2f = void (*)(GLfloat, GLfloat);
using PFN_glEnable = void (*)(GLenum);

static PFN_glGetString p_glGetString;
static PFN_glViewport p_glViewport;
static PFN_glClearColor p_glClearColor;
static PFN_glClear p_glClear;
static PFN_glDrawArrays p_glDrawArrays;
static PFN_glFinish p_glFinish;
static PFN_glReadPixels p_glReadPixels;
static PFN_glPixelStorei p_glPixelStorei;
static PFN_glGetError p_glGetError;
static PFN_glBegin p_glBegin;
static PFN_glEnd p_glEnd;
static PFN_glColor3f p_glColor3f;
static PFN_glVertex2f p_glVertex2f;
static PFNGLCREATESHADERPROC p_glCreateShader;
static PFNGLSHADERSOURCEPROC p_glShaderSource;
static PFNGLCOMPILESHADERPROC p_glCompileShader;
static PFNGLGETSHADERIVPROC p_glGetShaderiv;
static PFNGLGETSHADERINFOLOGPROC p_glGetShaderInfoLog;
static PFNGLCREATEPROGRAMPROC p_glCreateProgram;
static PFNGLATTACHSHADERPROC p_glAttachShader;
static PFNGLLINKPROGRAMPROC p_glLinkProgram;
static PFNGLGETPROGRAMIVPROC p_glGetProgramiv;
static PFNGLGETPROGRAMINFOLOGPROC p_glGetProgramInfoLog;
static PFNGLUSEPROGRAMPROC p_glUseProgram;
static PFNGLGETATTRIBLOCATIONPROC p_glGetAttribLocation;
static PFNGLENABLEVERTEXATTRIBARRAYPROC p_glEnableVertexAttribArray;
static PFNGLVERTEXATTRIBPOINTERPROC p_glVertexAttribPointer;
static PFN_glEnable p_glEnable;
static PFNGLGETUNIFORMLOCATIONPROC p_glGetUniformLocation;
static PFNGLUNIFORMMATRIX4FVPROC p_glUniformMatrix4fv;

template<typename T>
static bool get_gl_function(T& ptr, const char* name) {
    ptr = reinterpret_cast<T>(eglGetProcAddress(name));

    if (!ptr) {
        fprintf(stderr, "eglGetProcAddress(\"%s\") failed\n", name);
        return false;
    }

    return true;
}

static bool load_gl_functions() {
    return get_gl_function(p_glGetString, "glGetString") &&
           get_gl_function(p_glViewport, "glViewport") &&
           get_gl_function(p_glClearColor, "glClearColor") &&
           get_gl_function(p_glClear, "glClear") &&
           get_gl_function(p_glDrawArrays, "glDrawArrays") &&
           get_gl_function(p_glFinish, "glFinish") &&
           get_gl_function(p_glReadPixels, "glReadPixels") &&
           get_gl_function(p_glPixelStorei, "glPixelStorei") &&
           get_gl_function(p_glGetError, "glGetError") &&
           get_gl_function(p_glBegin, "glBegin") &&
           get_gl_function(p_glEnd, "glEnd") &&
           get_gl_function(p_glColor3f, "glColor3f") &&
           get_gl_function(p_glVertex2f, "glVertex2f") &&
           get_gl_function(p_glCreateShader, "glCreateShader") &&
           get_gl_function(p_glShaderSource, "glShaderSource") &&
           get_gl_function(p_glCompileShader, "glCompileShader") &&
           get_gl_function(p_glGetShaderiv, "glGetShaderiv") &&
           get_gl_function(p_glGetShaderInfoLog, "glGetShaderInfoLog") &&
           get_gl_function(p_glCreateProgram, "glCreateProgram") &&
           get_gl_function(p_glAttachShader, "glAttachShader") &&
           get_gl_function(p_glLinkProgram, "glLinkProgram") &&
           get_gl_function(p_glGetProgramiv, "glGetProgramiv") &&
           get_gl_function(p_glGetProgramInfoLog, "glGetProgramInfoLog") &&
           get_gl_function(p_glUseProgram, "glUseProgram") &&
           get_gl_function(p_glGetAttribLocation, "glGetAttribLocation") &&
           get_gl_function(p_glEnableVertexAttribArray, "glEnableVertexAttribArray") &&
           get_gl_function(p_glVertexAttribPointer, "glVertexAttribPointer") &&
           get_gl_function(p_glEnable, "glEnable") &&
           get_gl_function(p_glGetUniformLocation, "glGetUniformLocation") &&
           get_gl_function(p_glUniformMatrix4fv, "glUniformMatrix4fv");
}

static void flush_framebuffer() {
    void* flush_addr = framebuffer.data();
    size_t flush_size = framebuffer.size_bytes();
    IO_STATUS_BLOCK iosb;

    auto status = NtFlushVirtualMemory(NtCurrentProcess(), &flush_addr,
                                       &flush_size, &iosb);
    if (!success(status))
        print_stderr("NtFlushVirtualMemory returned {}\n", status);
}

static bool open_keyboard(unique_handle& keyboard) {
    OBJECT_ATTRIBUTES oa;
    UNICODE_STRING us;
    IO_STATUS_BLOCK iosb;
    NTSTATUS status;
    HANDLE h;

    static const u16string_view fn = u"/dev/keyboard";

    us.Length = us.MaximumLength = (uint16_t)(fn.size() * sizeof(char16_t));
    us.Buffer = (char16_t*)fn.data();

    memset(&oa, 0, sizeof(oa));
    oa.Length = sizeof(OBJECT_ATTRIBUTES);
    oa.ObjectName = &us;

    status = NtCreateFile(&h, FILE_READ_DATA | SYNCHRONIZE, &oa, &iosb,
                          nullptr, FILE_ATTRIBUTE_NORMAL, 0, FILE_OPEN,
                          0, nullptr, 0);
    if (!success(status)) {
        print_stderr("NtCreateFile returned {}\n", status);
        return false;
    }

    keyboard.reset(h);

    return true;
}

// drain any pending keyboard events without blocking, reporting whether
// Esc was among them
static bool esc_pressed(HANDLE keyboard) {
    IO_STATUS_BLOCK iosb;
    NTSTATUS status;

    while (true) {
        LARGE_INTEGER timeout;

        timeout.QuadPart = 0;

        status = NtWaitForSingleObject(keyboard, false, &timeout);

        if (status == nt_status::TIMEOUT)
            return false;
        else if (!success(status)) {
            print_stderr("NtWaitForSingleObject returned {}\n", status);
            return true;
        }

        input_event_with_id ev;

        status = NtReadFile(keyboard, 0, nullptr, nullptr, &iosb, &ev,
                            sizeof(ev), nullptr, nullptr);

        if (status == nt_status::ALERTED)
            continue;
        else if (!success(status)) {
            print_stderr("NtReadFile returned {}\n", status);
            return true;
        }

        if (ev.ev.type != evdev_type::EV_KEY)
            continue;

        if ((evdev_key_code)ev.ev.code == evdev_key_code::KEY_ESC)
            return true;
    }
}

static bool map_framebuffer() {
    OBJECT_ATTRIBUTES oa;
    UNICODE_STRING us;
    unique_handle sect;
    HANDLE h;
    nt_status status;
    MN_SECTION_FRAMEBUFFER_INFORMATION fb_info;
    IO_STATUS_BLOCK iosb;
    void* addr = nullptr;
    size_t size = 0, return_length;

    static const u16string_view fb = u"/dev/framebuffer";

    us.Length = us.MaximumLength = (uint16_t)(fb.size() * sizeof(char16_t));
    us.Buffer = (char16_t*)fb.data();

    memset(&oa, 0, sizeof(oa));
    oa.Length = sizeof(OBJECT_ATTRIBUTES);
    oa.ObjectName = &us;

    status = NtOpenSection(&h, SECTION_MAP_READ, &oa);
    if (!success(status)) {
        print_stderr("NtOpenSection returned {}\n", status);
        return false;
    }

    sect.reset(h);

    status = NtDeviceIoControlFile(sect.get(), 0, nullptr, nullptr, &iosb,
                                   IOCTL_MOXNOX_FB_CREATE_FRAMEBUFFER,
                                   nullptr, 0, &h, sizeof(h));
    if (!success(status)) {
        print_stderr("IOCTL_MOXNOX_FB_CREATE_FRAMEBUFFER returned {}\n", status);
        return false;
    }

    sect.reset(h);

    status = NtDeviceIoControlFile(sect.get(), 0, nullptr, nullptr, &iosb,
                                   IOCTL_MOXNOX_FB_SET_ACTIVE, nullptr, 0,
                                   nullptr, 0);
    if (!success(status)) {
        print_stderr("IOCTL_MOXNOX_FB_SET_ACTIVE returned {}\n", status);
        return false;
    }

    status = NtQuerySection(sect.get(), SECTION_INFORMATION_CLASS::MoxnoxFramebufferInformation,
                            &fb_info, sizeof(fb_info), &return_length);
    if (!success(status)) {
        print_stderr("NtQuerySection returned {}\n", status);
        return false;
    }

    status = NtMapViewOfSection(sect.get(), NtCurrentProcess(), &addr, 0, 0,
                                nullptr, &size, SECTION_INHERIT::ViewUnmap, 0,
                                PAGE_READWRITE);
    if (!success(status)) {
        print_stderr("NtMapViewOfSection returned {}\n", status);
        return false;
    }

    framebuffer = span((uint32_t*)addr, size / sizeof(uint32_t));

    fb_width = fb_info.width;
    fb_height = fb_info.height;
    fb_pixels_per_scan_line = fb_info.pixels_per_scan_line;

    return true;
}

static void check_gl_error(const char* stage) {
    GLenum err = p_glGetError();

    while (err != GL_NO_ERROR) {
        fprintf(stderr, "GL error %#x after %s\n", err, stage);
        err = p_glGetError();
    }
}

static GLuint compile_shader(GLenum type, const char* source) {
    GLuint shader = p_glCreateShader(type);
    GLint ok;

    p_glShaderSource(shader, 1, &source, nullptr);
    p_glCompileShader(shader);

    p_glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[1024];
        p_glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        fprintf(stderr, "shader compilation failed:\n%s\n", log);
        return 0;
    }

    return shader;
}

static const char vertex_shader_src[] =
    "#version 120\n"
    "attribute vec2 pos;\n"
    "attribute vec3 colour;\n"
    "varying vec3 v_colour;\n"
    "void main() {\n"
    "    gl_Position = vec4(pos, 0.0, 1.0);\n"
    "    v_colour = colour;\n"
    "}\n";

static const char fragment_shader_src[] =
    "#version 120\n"
    "varying vec3 v_colour;\n"
    "void main() {\n"
    "    gl_FragColor = vec4(v_colour, 1.0);\n"
    "}\n";

// no vertex attributes at all: positions synthesized from gl_VertexID
static const char vertexid_vs_src[] =
    "#version 130\n"
    "void main() {\n"
    "    vec2 p;\n"
    "    if (gl_VertexID == 0)\n"
    "        p = vec2(0.4, -0.8);\n"
    "    else if (gl_VertexID == 1)\n"
    "        p = vec2(0.9, -0.8);\n"
    "    else\n"
    "        p = vec2(0.65, 0.8);\n"
    "    gl_Position = vec4(p, 0.0, 1.0);\n"
    "}\n";

static const char vertexid_fs_src[] =
    "#version 130\n"
    "void main() {\n"
    "    gl_FragColor = vec4(1.0, 1.0, 0.0, 1.0);\n"
    "}\n";

static GLuint link_program(const char* vs_src, const char* fs_src) {
    GLuint vs, fs, prog;
    GLint ok;

    vs = compile_shader(GL_VERTEX_SHADER, vs_src);
    fs = compile_shader(GL_FRAGMENT_SHADER, fs_src);
    if (vs == 0 || fs == 0)
        return 0;

    prog = p_glCreateProgram();
    p_glAttachShader(prog, vs);
    p_glAttachShader(prog, fs);
    p_glLinkProgram(prog);

    p_glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[1024];
        p_glGetProgramInfoLog(prog, sizeof(log), nullptr, log);
        fprintf(stderr, "program link failed:\n%s\n", log);
        return 0;
    }

    return prog;
}

static bool draw_triangle() {
    GLuint prog, vertexid_prog;
    GLint pos_loc, colour_loc;

    prog = link_program(vertex_shader_src, fragment_shader_src);
    vertexid_prog = link_program(vertexid_vs_src, vertexid_fs_src);
    if (prog == 0 || vertexid_prog == 0)
        return false;

    p_glViewport(0, 0, (GLsizei)fb_width, (GLsizei)fb_height);

    p_glClearColor(0.1f, 0.1f, 0.2f, 1.0f);
    p_glClear(GL_COLOR_BUFFER_BIT);
    check_gl_error("glClear");

    // 1: fixed-function immediate mode, left third, white
    p_glBegin(GL_TRIANGLES);
    p_glColor3f(1.0f, 1.0f, 1.0f);
    p_glVertex2f(-0.9f, -0.8f);
    p_glVertex2f(-0.4f, -0.8f);
    p_glVertex2f(-0.65f, 0.8f);
    p_glEnd();
    check_gl_error("fixed-function draw");

    // 2: shader with vertex attributes, centre, RGB gradient
    p_glUseProgram(prog);
    check_gl_error("glUseProgram");

    static const GLfloat verts[] = {
        // x, y, r, g, b
        -0.25f, -0.8f, 1.0f, 0.0f, 0.0f,
         0.25f, -0.8f, 0.0f, 1.0f, 0.0f,
         0.0f,   0.8f, 0.0f, 0.0f, 1.0f,
    };

    pos_loc = p_glGetAttribLocation(prog, "pos");
    colour_loc = p_glGetAttribLocation(prog, "colour");

    printf("attrib locations: pos=%d colour=%d\n", pos_loc, colour_loc);

    if (pos_loc < 0 || colour_loc < 0) {
        fprintf(stderr, "attribute lookup failed\n");
        return false;
    }

    p_glEnableVertexAttribArray((GLuint)pos_loc);
    p_glVertexAttribPointer((GLuint)pos_loc, 2, GL_FLOAT, GL_FALSE,
                            5 * sizeof(GLfloat), verts);
    p_glEnableVertexAttribArray((GLuint)colour_loc);
    p_glVertexAttribPointer((GLuint)colour_loc, 3, GL_FLOAT, GL_FALSE,
                            5 * sizeof(GLfloat), &verts[2]);
    check_gl_error("vertex attrib setup");

    p_glDrawArrays(GL_TRIANGLES, 0, 3);
    check_gl_error("glDrawArrays");

    // 3: shader without any vertex attributes, right third, yellow
    p_glUseProgram(vertexid_prog);
    p_glDrawArrays(GL_TRIANGLES, 0, 3);
    check_gl_error("gl_VertexID draw");

    p_glFinish();
    check_gl_error("glFinish");

    return true;
}

static void copy_to_framebuffer(bool report) {
    // read back in BGRX, which matches the framebuffer layout; static so
    // the animation loop is not thrashing the heap every frame
    static vector<uint32_t> pixels((size_t)fb_pixels_per_scan_line * fb_height);

    p_glPixelStorei(GL_PACK_ROW_LENGTH, (GLint)fb_pixels_per_scan_line);
    p_glReadPixels(0, 0, (GLsizei)fb_width, (GLsizei)fb_height,
                   GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, pixels.data());
    check_gl_error("glReadPixels");

    if (report) {
        // sample one point inside each of the three triangles
        size_t mid_row = (size_t)(fb_height / 2) * fb_pixels_per_scan_line;
        printf("fixed-function pixel: %#010x (expect white 0xffffffff)\n",
               pixels[mid_row + (size_t)((float)fb_width * 0.175f)]);
        printf("attribute pixel:      %#010x (expect a red/green/blue mix)\n",
               pixels[mid_row + fb_width / 2]);
        printf("gl_VertexID pixel:    %#010x (expect yellow 0xffffff00)\n",
               pixels[mid_row + (size_t)((float)fb_width * 0.825f)]);
        printf("(the clear colour is 0xff191933)\n");
    }

    // GL's origin is bottom-left, the framebuffer's is top-left
    for (unsigned int y = 0; y < fb_height; y++) {
        memcpy(&framebuffer[(size_t)y * fb_pixels_per_scan_line],
               &pixels[(size_t)(fb_height - 1 - y) * fb_pixels_per_scan_line],
               fb_width * sizeof(uint32_t));
    }

    flush_framebuffer();
}

// ---------------------------------------------------------------------------
// rotating cube
// ---------------------------------------------------------------------------

static const char cube_vs_src[] =
    "#version 120\n"
    "attribute vec3 pos;\n"
    "attribute vec3 colour;\n"
    "uniform mat4 mvp;\n"
    "varying vec3 v_colour;\n"
    "void main() {\n"
    "    gl_Position = mvp * vec4(pos, 1.0);\n"
    "    v_colour = colour;\n"
    "}\n";

struct mat4 {
    float m[16]; // column-major
};

static mat4 mat4_mul(const mat4& a, const mat4& b) {
    mat4 r;

    for (unsigned int col = 0; col < 4; col++) {
        for (unsigned int row = 0; row < 4; row++) {
            float sum = 0.0f;

            for (unsigned int k = 0; k < 4; k++) {
                sum += a.m[(k * 4) + row] * b.m[(col * 4) + k];
            }

            r.m[(col * 4) + row] = sum;
        }
    }

    return r;
}

static mat4 mat4_perspective(float fovy, float aspect, float near_z,
                             float far_z) {
    float f = 1.0f / tanf(fovy / 2.0f);
    mat4 r = {};

    r.m[0] = f / aspect;
    r.m[5] = f;
    r.m[10] = (far_z + near_z) / (near_z - far_z);
    r.m[11] = -1.0f;
    r.m[14] = 2.0f * far_z * near_z / (near_z - far_z);

    return r;
}

static mat4 mat4_rotate_x(float angle) {
    float c = cosf(angle), s = sinf(angle);
    mat4 r = {};

    r.m[0] = 1.0f;
    r.m[5] = c;
    r.m[6] = s;
    r.m[9] = -s;
    r.m[10] = c;
    r.m[15] = 1.0f;

    return r;
}

static mat4 mat4_rotate_y(float angle) {
    float c = cosf(angle), s = sinf(angle);
    mat4 r = {};

    r.m[0] = c;
    r.m[2] = -s;
    r.m[5] = 1.0f;
    r.m[8] = s;
    r.m[10] = c;
    r.m[15] = 1.0f;

    return r;
}

static mat4 mat4_translate_z(float z) {
    mat4 r = {};

    r.m[0] = 1.0f;
    r.m[5] = 1.0f;
    r.m[10] = 1.0f;
    r.m[14] = z;
    r.m[15] = 1.0f;

    return r;
}

// x, y, z, r, g, b; one colour per face
static const GLfloat cube_verts[] = {
    // +Z, red
    -1.0f, -1.0f,  1.0f, 1.0f, 0.0f, 0.0f,
     1.0f, -1.0f,  1.0f, 1.0f, 0.0f, 0.0f,
     1.0f,  1.0f,  1.0f, 1.0f, 0.0f, 0.0f,
    -1.0f, -1.0f,  1.0f, 1.0f, 0.0f, 0.0f,
     1.0f,  1.0f,  1.0f, 1.0f, 0.0f, 0.0f,
    -1.0f,  1.0f,  1.0f, 1.0f, 0.0f, 0.0f,
    // -Z, green
    -1.0f, -1.0f, -1.0f, 0.0f, 1.0f, 0.0f,
     1.0f,  1.0f, -1.0f, 0.0f, 1.0f, 0.0f,
     1.0f, -1.0f, -1.0f, 0.0f, 1.0f, 0.0f,
    -1.0f, -1.0f, -1.0f, 0.0f, 1.0f, 0.0f,
    -1.0f,  1.0f, -1.0f, 0.0f, 1.0f, 0.0f,
     1.0f,  1.0f, -1.0f, 0.0f, 1.0f, 0.0f,
    // +X, blue
     1.0f, -1.0f, -1.0f, 0.0f, 0.0f, 1.0f,
     1.0f,  1.0f,  1.0f, 0.0f, 0.0f, 1.0f,
     1.0f, -1.0f,  1.0f, 0.0f, 0.0f, 1.0f,
     1.0f, -1.0f, -1.0f, 0.0f, 0.0f, 1.0f,
     1.0f,  1.0f, -1.0f, 0.0f, 0.0f, 1.0f,
     1.0f,  1.0f,  1.0f, 0.0f, 0.0f, 1.0f,
    // -X, yellow
    -1.0f, -1.0f, -1.0f, 1.0f, 1.0f, 0.0f,
    -1.0f, -1.0f,  1.0f, 1.0f, 1.0f, 0.0f,
    -1.0f,  1.0f,  1.0f, 1.0f, 1.0f, 0.0f,
    -1.0f, -1.0f, -1.0f, 1.0f, 1.0f, 0.0f,
    -1.0f,  1.0f,  1.0f, 1.0f, 1.0f, 0.0f,
    -1.0f,  1.0f, -1.0f, 1.0f, 1.0f, 0.0f,
    // +Y, cyan
    -1.0f,  1.0f, -1.0f, 0.0f, 1.0f, 1.0f,
    -1.0f,  1.0f,  1.0f, 0.0f, 1.0f, 1.0f,
     1.0f,  1.0f,  1.0f, 0.0f, 1.0f, 1.0f,
    -1.0f,  1.0f, -1.0f, 0.0f, 1.0f, 1.0f,
     1.0f,  1.0f,  1.0f, 0.0f, 1.0f, 1.0f,
     1.0f,  1.0f, -1.0f, 0.0f, 1.0f, 1.0f,
    // -Y, magenta
    -1.0f, -1.0f, -1.0f, 1.0f, 0.0f, 1.0f,
     1.0f, -1.0f,  1.0f, 1.0f, 0.0f, 1.0f,
    -1.0f, -1.0f,  1.0f, 1.0f, 0.0f, 1.0f,
    -1.0f, -1.0f, -1.0f, 1.0f, 0.0f, 1.0f,
     1.0f, -1.0f, -1.0f, 1.0f, 0.0f, 1.0f,
     1.0f, -1.0f,  1.0f, 1.0f, 0.0f, 1.0f,
};

static bool run_cube() {
    unique_handle keyboard;
    GLuint prog;
    GLint pos_loc, colour_loc, mvp_loc;

    if (!open_keyboard(keyboard))
        return false;

    prog = link_program(cube_vs_src, fragment_shader_src);
    if (prog == 0)
        return false;

    pos_loc = p_glGetAttribLocation(prog, "pos");
    colour_loc = p_glGetAttribLocation(prog, "colour");
    mvp_loc = p_glGetUniformLocation(prog, "mvp");

    if (pos_loc < 0 || colour_loc < 0 || mvp_loc < 0) {
        fprintf(stderr, "cube attribute/uniform lookup failed\n");
        return false;
    }

    p_glUseProgram(prog);

    p_glEnableVertexAttribArray((GLuint)pos_loc);
    p_glVertexAttribPointer((GLuint)pos_loc, 3, GL_FLOAT, GL_FALSE,
                            6 * sizeof(GLfloat), cube_verts);
    p_glEnableVertexAttribArray((GLuint)colour_loc);
    p_glVertexAttribPointer((GLuint)colour_loc, 3, GL_FLOAT, GL_FALSE,
                            6 * sizeof(GLfloat), &cube_verts[3]);

    p_glEnable(GL_DEPTH_TEST);
    p_glEnable(GL_CULL_FACE);
    check_gl_error("cube setup");

    printf("rotating cube: press Esc to exit\n");

    auto proj = mat4_perspective(1.0f, (float)fb_width / (float)fb_height,
                                 0.1f, 100.0f);
    auto view = mat4_translate_z(-4.0f);

    auto start = clock();
    auto fps_start = start;
    unsigned int frames = 0;

    while (!esc_pressed(keyboard.get())) {
        float t = (float)(clock() - start) / (float)CLOCKS_PER_SEC;

        auto model = mat4_mul(mat4_rotate_y(t), mat4_rotate_x(t * 0.7f));
        auto mvp = mat4_mul(proj, mat4_mul(view, model));

        p_glClearColor(0.1f, 0.1f, 0.2f, 1.0f);
        p_glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        p_glUniformMatrix4fv(mvp_loc, 1, GL_FALSE, mvp.m);
        p_glDrawArrays(GL_TRIANGLES, 0, 36);

        p_glFinish();

        copy_to_framebuffer(false);

        frames++;

        if (frames % 100 == 0) {
            auto now = clock();

            printf("%.1f fps\n", 100.0 * (double)CLOCKS_PER_SEC /
                                 (double)(now - fps_start));
            fps_start = now;
        }
    }

    check_gl_error("cube loop");

    return true;
}

int main() {
    if (!map_framebuffer())
        return 1;

    EGLDisplay dpy = eglGetPlatformDisplay(EGL_PLATFORM_SURFACELESS_MESA,
                                           EGL_DEFAULT_DISPLAY, nullptr);
    if (dpy == EGL_NO_DISPLAY) {
        fprintf(stderr, "eglGetPlatformDisplay failed\n");
        return 1;
    }

    EGLint major, minor;
    if (!eglInitialize(dpy, &major, &minor)) {
        fprintf(stderr, "eglInitialize failed (error %#x)\n", eglGetError());
        return 1;
    }

    printf("EGL %d.%d (%s)\n", major, minor, eglQueryString(dpy, EGL_VERSION));

    static const EGLint config_attribs[] = {
        EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_DEPTH_SIZE, 24,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT,
        EGL_NONE
    };

    EGLConfig config;
    EGLint num_configs;
    if (!eglChooseConfig(dpy, config_attribs, &config, 1, &num_configs) ||
        num_configs == 0) {
        fprintf(stderr, "eglChooseConfig failed (error %#x)\n", eglGetError());
        return 1;
    }

    if (!eglBindAPI(EGL_OPENGL_API)) {
        fprintf(stderr, "eglBindAPI failed (error %#x)\n", eglGetError());
        return 1;
    }

    const EGLint pbuffer_attribs[] = {
        EGL_WIDTH, (EGLint)fb_width,
        EGL_HEIGHT, (EGLint)fb_height,
        EGL_NONE
    };

    EGLSurface surface = eglCreatePbufferSurface(dpy, config, pbuffer_attribs);
    if (surface == EGL_NO_SURFACE) {
        fprintf(stderr, "eglCreatePbufferSurface failed (error %#x)\n",
                eglGetError());
        return 1;
    }

    EGLContext context = eglCreateContext(dpy, config, EGL_NO_CONTEXT, nullptr);
    if (context == EGL_NO_CONTEXT) {
        fprintf(stderr, "eglCreateContext failed (error %#x)\n", eglGetError());
        return 1;
    }

    if (!eglMakeCurrent(dpy, surface, surface, context)) {
        fprintf(stderr, "eglMakeCurrent failed (error %#x)\n", eglGetError());
        return 1;
    }

    if (!load_gl_functions())
        return 1;

    printf("GL_RENDERER: %s\n", p_glGetString(GL_RENDERER));
    printf("GL_VERSION: %s\n", p_glGetString(GL_VERSION));

    if (!draw_triangle())
        return 1;

    copy_to_framebuffer(true);

    if (!run_cube())
        return 1;

    eglMakeCurrent(dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    eglDestroyContext(dpy, context);
    eglDestroySurface(dpy, surface);
    eglTerminate(dpy);

    printf("Mesa test complete!\n");
    return 0;
}
