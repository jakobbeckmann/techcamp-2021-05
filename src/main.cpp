#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#define STB_IMAGE_IMPLEMENTATION

#include "STDIMAGE/std_image.h"

#include <math.h>
#include <iostream>
#include <assert.h>
#include <fstream>

#include "Shader.h"
#include "camera.h"
#include "window.h"
#include "datamanager.h"
#include "filesize.h"

#include "buffers/buffer.h"
#include "buffers/indexbuffer.h"
#include "buffers/vertexarray.h"

#include <emscripten.h>
#include <emscripten/html5.h>
#include <cmath>
#include <functional>


std::function<void()> registered_loop;

void loop_iteration() {
    registered_loop();
}

int main() {

    std::cout << "started" << std::endl;
    // ---------------------------
    // Data management
    int startIdx = 50;
    int endIdx = 200;
    int stepSize = 50;

    std::vector<std::string> outSteps = arange(startIdx, endIdx, stepSize);
    DataManager fileReader(outSteps);
    fileReader.loadAllFiles();

    size_t n_timesteps = fileReader.getNTimesteps();
    size_t n_vertices = fileReader.getNumberOfParticles();


    // ---------------------------
    // Window initialization
    Window window("sphGL", 500, 500);
    Camera camera(glm::vec3(0.0f, 0.0f, 20.0f));
    window.attachCamera(camera);

    // init glew
    glewExperimental = GL_TRUE;
    GLenum err = glewInit();

    if (err != GLEW_OK) {
        glfwTerminate();
        throw std::runtime_error(std::string("Could initialize GLEW, error = ") +
                                 (const char *) glewGetErrorString(err));
    }
    unsigned int timestep = 0;
    std::string currentOutputStep = outSteps[timestep];

    // ---------------------------
    // Initialize buffers & shader program
    VertexArray VAO;
    Buffer *VBO_pos = new Buffer(fileReader.m_Position.data(), fileReader.getPosSize(), 3);
    Buffer *VBO_dens = new Buffer(fileReader.m_Density.data(), fileReader.getDensSize(), 1);

    VAO.addBuffer(VBO_pos, 0);
    VAO.addBuffer(VBO_dens, 2);

    // Creating a shader program
    Shader shaderProgram("../shaders/shader.vert", "../shaders/shader.frag");
    shaderProgram.enable();

    float pointSize = 1.0f;

    // ---------------------------
    // TW section
    float fps = 0.0f;

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);

    // TODO: Do we need this? Seems not to be defined for webgl.
    // glEnable(GL_PROGRAM_POINT_SIZE);

    int numberOfRenders = 0;
    float timeValue = 0.0f;
    float lastTimeValue = 0.0f;
    float deltaTime = 0.0f;
    float lastFrame = 0.0f;

    // ---------------------------
    // main rendering loop
    registered_loop = [&]() {

        deltaTime = glfwGetTime() - lastFrame;
        lastFrame = timeValue;

        window.detectWindowDimensionChange();
        window.processInput(deltaTime, timestep, n_timesteps - 1, pointSize);
        window.clear();

        glm::mat4 model, trans;
        shaderProgram.setMat4("model", model);
        shaderProgram.setMat4("transform", trans);

        glm::mat4 view = camera.GetViewMatrix();
        shaderProgram.setMat4("view", view);

        glm::mat4 projection = glm::perspective(glm::radians(camera.Zoom),
                                                (float) window.getWidth() / (float) window.getHeight(), 0.01f, 100.0f);
        shaderProgram.setMat4("projection", projection);

        window.updatePointSize(pointSize);
        shaderProgram.setFloat("pointSize", pointSize);

        unsigned int timestepDirection = window.queryBufferUpdate();

        if (timestepDirection != Window::timeStepDirection::NOUPDATE) {
            if (timestepDirection == Window::timeStepDirection::BACKSTEP && timestep > 0) {
                timestep--;
            } else if (timestepDirection == Window::timeStepDirection::FRONTSTEP &&
                       timestep < fileReader.getNTimesteps() - 1) {
                timestep++;
            }

            VBO_pos->updateBuffer(fileReader.getPosSize() / fileReader.getNTimesteps(),
                                  fileReader.getPositionP(timestep));
            VBO_dens->updateBuffer(fileReader.getDensSize() / fileReader.getNTimesteps(),
                                   fileReader.getDensityP(timestep));
        }

        //-------------
//		std::cout << "before draw" << std::endl;
        // draw call
        VAO.bind();
        glDrawArrays(GL_POINTS, 0, n_vertices);
        VAO.unbind();

//		std::cout << "after draw" << std::endl;

        if (numberOfRenders % 1000 == 0) {
            fps = 1000 / (timeValue - lastTimeValue);
            lastTimeValue = timeValue;
        }

        numberOfRenders += 1;

        window.update();
        glfwPollEvents();

    };
    emscripten_set_main_loop(loop_iteration, 0, 1);

    while (true) {
        loop_iteration();
    }

    VAO.~VertexArray();
    glfwTerminate();

    return 0;
}


// TODO: outsource this function to where it belongs
unsigned int createTexture() {
    //
    unsigned int texture; // also multiple textures possible, then create list and pass to glGenTextures
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);

    // set tex wrapping options (s, t, r) -> (x, y, z)
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    // set tex filterung options
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST); // when making image smaller
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR); // when largening image

    // load image and match on texture
    int width, height, nrChannels;
    unsigned char *img_data = stbi_load("Textures/container.jpg", &width, &height, &nrChannels, 0);
    if (img_data) { // valid c++ pointers are implicitely converted to boolean true
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, img_data);
        glGenerateMipmap(GL_TEXTURE_2D);
    } else {
        std::cout << "Failed to load image." << std::endl;
    }
    stbi_image_free(img_data); // free image data

    return texture;
}
