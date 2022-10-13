#include "ShaderProgram.h"

#include <cutils/log.h>

//#define DEBUG_GL

#ifdef DEBUG_GL
#define CHECK() \
{\
    GLenum err = glGetError(); \
    if (err != GL_NO_ERROR) \
    {\
        ALOGD("%d:glGetError returns %d", __LINE__, err); \
    }\
}
#else
#define CHECK()
#endif

ShaderProgram::ShaderProgram(const char* pComputeSource) {
    mProgram = createComputeProgram(pComputeSource);
    mIsComputeProgram = true;
}

ShaderProgram::ShaderProgram(const char* pVertexSource, const char* pFragmentSource) {
    mProgram = createGraphicsProgram(pVertexSource, pFragmentSource);
}

ShaderProgram::~ShaderProgram() {
    //ALOGD("ShaderProgram::~ShaderProgram() %p", this);
    if (mProgram) {
        glDeleteProgram(mProgram);
        CHECK();
    }
    for (auto& it: mShaders) {
        glDeleteShader(it.second);
        CHECK();
    }
}

GLint ShaderProgram::getUniformLocation(const char* name) {
    std::string s = name;
    if (mUniformLocs.find(s) == mUniformLocs.end()) {
        GLint loc = glGetUniformLocation(mProgram, s.c_str());
        mUniformLocs.insert(std::make_pair(s, loc));
    }
    return mUniformLocs.at(s);
}

GLuint ShaderProgram::loadShader(GLenum shaderType, const char* pSource) {
    GLuint shader = glCreateShader(shaderType);
    if (shader) {
        glShaderSource(shader, 1, &pSource, NULL);
        glCompileShader(shader);
        GLint compiled = 0;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
        if (!compiled) {
            GLint infoLen = 0;
            glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infoLen);
            if (infoLen) {
                char* buf = (char*) malloc(infoLen);
                if (buf) {
                    glGetShaderInfoLog(shader, infoLen, NULL, buf);
                    ALOGE("Could not compile shader %d:\n%s", shaderType, buf);
                    free(buf);
                }
                glDeleteShader(shader);
                shader = 0;
            }
        }
    }
    mShaders.insert(std::make_pair(shaderType, shader));
    return shader;
}

GLuint ShaderProgram::createComputeProgram(const char* pComputeSource) {
    GLuint computeShader = loadShader(GL_COMPUTE_SHADER, pComputeSource);
    if (!computeShader) {
        return 0;
    }

    GLuint program = glCreateProgram();
    if (program) {
        glAttachShader(program, computeShader);
        glLinkProgram(program);
        GLint linkStatus = GL_FALSE;
        glGetProgramiv(program, GL_LINK_STATUS, &linkStatus);
        if (linkStatus != GL_TRUE) {
            GLint bufLength = 0;
            glGetProgramiv(program, GL_INFO_LOG_LENGTH, &bufLength);
            if (bufLength) {
                char* buf = (char*) malloc(bufLength);
                if (buf) {
                    glGetProgramInfoLog(program, bufLength, NULL, buf);
                    ALOGE("Could not link program:\n%s", buf);
                    free(buf);
                }
            }
            glDeleteProgram(program);
            program = 0;
        }
    }
    return program;
}

GLuint ShaderProgram::createGraphicsProgram(const char* pVertexSource, const char* pFragmentSource) {
    GLuint vertexShader = loadShader(GL_VERTEX_SHADER, pVertexSource);
    if (!vertexShader) {
        return 0;
    }

    GLuint pixelShader = loadShader(GL_FRAGMENT_SHADER, pFragmentSource);
    if (!pixelShader) {
        return 0;
    }

    GLuint program = glCreateProgram();
    if (program) {
        glAttachShader(program, vertexShader);
        CHECK();
        glAttachShader(program, pixelShader);
        CHECK();
        glLinkProgram(program);
        GLint linkStatus = GL_FALSE;
        glGetProgramiv(program, GL_LINK_STATUS, &linkStatus);
        if (linkStatus != GL_TRUE) {
            GLint bufLength = 0;
            glGetProgramiv(program, GL_INFO_LOG_LENGTH, &bufLength);
            if (bufLength) {
                char* buf = (char*) malloc(bufLength);
                if (buf) {
                    glGetProgramInfoLog(program, bufLength, NULL, buf);
                    ALOGE("Could not link program:\n%s", buf);
                    free(buf);
                }
            }
            glDeleteProgram(program);
            program = 0;
        }
    }
    return program;
}
