#include <glad/glad.h>
#include <GLFW/glfw3.h>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/quaternion.hpp>


#include <utils/shader.h>
#include <utils/camera.h>
#include <utils/model.h>
#include <chrono>

#include <iostream>


static float dy_factor = 1.0f;
static float frame_time = 0.0f;
static glm::vec2 m_pos = glm::vec2(0.f);
static glm::vec2 m_delta = glm::vec2(0.f);
static bool m_pressed = false;
//static int _global_cmp_slot_cnt_ = 0;
#define Render_rs 0

// fluid definations
// ---------------
#define CellSize (1.25f)
#define WORKGROUP_SIZE 16

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void mouse_button_callback(GLFWwindow* window, int button, int action, int mods);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
void processInput(GLFWwindow *window);
unsigned int TextureFromFile(const string path, bool gamma = 0);
//void renderQuad();

// renderQuad() renders a 1x1 XY quad in NDC
// -----------------------------------------
struct Quad {
    unsigned int quadVAO = 0;
    unsigned int quadVBO;
    void renderQuad(float resolution_fac = 1.0)
    {
        if (quadVAO == 0)
        {
            float quadVertices[] = {
                // positions        // texture Coords
                -1.0f,  1.0f, 0.0f, 0.0f * resolution_fac, 1.0f * resolution_fac,
                -1.0f, -1.0f, 0.0f, 0.0f * resolution_fac, 0.0f * resolution_fac,
                 1.0f,  1.0f, 0.0f, 1.0f * resolution_fac, 1.0f * resolution_fac,
                 1.0f, -1.0f, 0.0f, 1.0f * resolution_fac, 0.0f * resolution_fac,
            };
            // setup plane VAO
            glGenVertexArrays(1, &quadVAO);
            glGenBuffers(1, &quadVBO);
            glBindVertexArray(quadVAO);
            glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
            glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);
            glEnableVertexAttribArray(0);
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
            glEnableVertexAttribArray(1);
            glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
        }
        glBindVertexArray(quadVAO);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        glBindVertexArray(0);
    }
    void destroyQuad() {
        glDeleteBuffers(1, &quadVAO);
        glDeleteBuffers(1, &quadVBO);
    }
    Quad() {};
};

// settings
const unsigned int SCR_WIDTH = 1024;
const unsigned int SCR_HEIGHT = 1024;

// camera
Camera camera(glm::vec3(0.0f, 0.0f, 5.0f));
float lastX = (float)SCR_WIDTH / 2.0;
float lastY = (float)SCR_HEIGHT / 2.0;
bool firstMouse = true;

// timing
float deltaTime = 0.0f;
float lastFrame = 0.0f;

Quad my_quad;

void createFluidTexture(GLuint& _tex) {
    glGenTextures(1, &_tex);
    glBindTexture(GL_TEXTURE_2D, _tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, 1024, 1024, 0, GL_RGBA, GL_FLOAT,
        NULL);

}

void bindTexture_image(GLuint _tex, int slot) {
    glBindImageTexture(slot, _tex, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA32F);
}

class fluidTexturePair {

public:  
    
    GLuint ping_;
    GLuint pong_;  

    fluidTexturePair() { create(); }

    void create() {
        createFluidTexture(ping_);
        createFluidTexture(pong_);
    }

    void swap() {
        auto _i_ = ping_;
        ping_ = pong_;
        pong_ = _i_;
    }

private:
    
    void createFluidTexture(GLuint& _tex) {
        glGenTextures(1, &_tex);
        glBindTexture(GL_TEXTURE_2D, _tex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, 1024, 1024, 0, GL_RGBA, GL_FLOAT,
            NULL);

        std::vector<float> data(4 * 1024 * 1024, 0.0f);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, 1024, 1024, 0, GL_RGBA, GL_FLOAT, data.data());
        glGenerateMipmap(GL_TEXTURE_2D);
        //glBindImageTexture(_global_cmp_slot_cnt_++, _tex, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA32F);
    }
    
};

int main()
{
    //_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
    // glfw: initialize and configure
    // ------------------------------
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4.3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 4.3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif
    
    // glfw window creation
    // --------------------
    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Y R WE STILL HERE ???", NULL, NULL);
    if (window == NULL)
    {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    glfwSetScrollCallback(window, scroll_callback);

    // tell GLFW to capture our mouse
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);

    // glad: load all OpenGL function pointers
    // ---------------------------------------
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    // tell stb_image.h to flip loaded texture's on the y-axis (before loading model).
    stbi_set_flip_vertically_on_load(true);

    // configure global opengl state
    // -----------------------------
    glEnable(GL_DEPTH_TEST);

    // build and compile shaders
    // -------------------------

    Shader shaderCustomBuffer("../res/custom_buffer.vs", "../res/custom_buffer.fs");


    // fluid shaders
    // ---------------

    Shader shaderFluidAdvect("../res/advect.comp");
    Shader shaderFluidVorticity("../res/vorticity.comp");
    Shader shaderFluidApplyBuo("../res/applyBuoyancy.comp");
    Shader shaderFluidApplyImp("../res/applyImpulse.comp");
    Shader shaderFluidCmpDiver("../res/computeDivergence.comp");
    Shader shaderFluidJacobi("../res/Jacobi.comp");
    Shader shaderFluidSubtractGrad("../res/subtractGradient.comp");
    Shader shaderFluidClear("../res/clear.comp");
    Shader shaderFluidPush("../res/push.comp");

    // load some custom textures
    // -----------

    GLuint Obstacle = TextureFromFile("../res/obstacle.png");


    // fluid sim init
    // --------

    fluidTexturePair Velocity;
    fluidTexturePair Density;
    fluidTexturePair Temperature;
    fluidTexturePair Pressure;
    GLuint Divergence;
    createFluidTexture(Divergence);

    static const float AmbientTemperature = 0.0f;
    static const float ImpulseTemperature = 1.6f;
    static const float ImpulseDensity = 0.2f;
    static const int NumJacobiIterations = 40;
    static const float TimeStep = 0.05f;
    static const float SmokeBuoyancy = 1.0f;
    static const float SmokeWeight = 0.05f;
    static const float GradientScale = 1.125f / CellSize;
    static const float TemperatureDissipation = 0.999f;//0.99
    static const float VelocityDissipation = 0.999f;//0.99
    static const float DensityDissipation = 0.9999f;//0.9999
    static const glm::vec2 ImpulseLocation = glm::vec2(512, 50);


    //------------compute shader texture----------------

    int tex_w = 1024, tex_h = 1024;
    GLuint cmp_tex;
    createFluidTexture(cmp_tex);
   
    GLuint cmp_tex2;
    createFluidTexture(cmp_tex2);
   
    //------------compute shader texture----------------
    //------------compute shader work group-------------

    int work_grp_cnt[3];

    glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 0, &work_grp_cnt[0]);
    glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 1, &work_grp_cnt[1]);
    glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 2, &work_grp_cnt[2]);
    printf("max global (total) work group counts x:%i y:%i z:%i\n",
        work_grp_cnt[0], work_grp_cnt[1], work_grp_cnt[2]);

    int work_grp_size[3];

    glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 0, &work_grp_size[0]);
    glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 1, &work_grp_size[1]);
    glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 2, &work_grp_size[2]);

    printf("max local (in one shader) work group sizes x:%i y:%i z:%i\n",
        work_grp_size[0], work_grp_size[1], work_grp_size[2]);

    int work_grp_inv;
    glGetIntegerv(GL_MAX_COMPUTE_WORK_GROUP_INVOCATIONS, &work_grp_inv);
    printf("max local work group invocations %i\n", work_grp_inv);

    //------------compute shader work group-------------

    // render loop
    // -----------

    auto _pre_timer = std::chrono::steady_clock::now();

#if Render_rs
    //realsense initialize
    // Declare RealSense pipeline, encapsulating the actual device and sensors
    rs2::pipeline pipe;
    // Create a configuration for configuring the pipeline with a non default profile
    rs2::config cfg;
    // Add pose stream
    cfg.enable_stream(RS2_STREAM_POSE, RS2_FORMAT_6DOF);
    // Start pipeline with chosen configuration
    pipe.start(cfg);
#endif // Render_rs



    while (!glfwWindowShouldClose(window))
    {

#if Render_rs
        // Wait for the next set of frames from the camera
        auto frames = pipe.wait_for_frames();
        // Get a frame from the pose stream
        auto f = frames.first_or_default(RS2_STREAM_POSE);
        // Cast the frame to pose_frame and get its data
        auto pose_data = f.as<rs2::pose_frame>().get_pose_data();

        std::cout << "x : " << pose_data.translation.x << ", y : " << pose_data.translation.y <<
            ", z : " << pose_data.translation.z << std::endl;
#endif

        glViewport(0, 0, SCR_WIDTH * dy_factor, SCR_WIDTH * dy_factor);
        auto _cur_timer = std::chrono::steady_clock::now();
        auto _elapsed_time = std::chrono::duration_cast<std::chrono::milliseconds>(_cur_timer - _pre_timer).count();
       // std::cout << 1000.0f / _elapsed_time << std::endl;
        _pre_timer = _cur_timer;

        // per-frame time logic
        // --------------------
        frame_time = glfwGetTime();
        float currentFrame = glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;
        //std::cout << "GLFW time : " << deltaTime << std::endl;

        // input
        // -----
        processInput(window);

        // fluid pipeline
        // -----

        // velocity advection
        shaderFluidAdvect.use();
        bindTexture_image(Velocity.pong_, 0);// output -- velocity pong
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, Velocity.ping_);
        shaderFluidAdvect.setInt("velocityPing", 1);// input -- velocity ping 
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, Velocity.ping_);
        shaderFluidAdvect.setInt("source", 2);// input -- source ping 
        glActiveTexture(GL_TEXTURE3);
        glBindTexture(GL_TEXTURE_2D, Obstacle);
        shaderFluidAdvect.setInt("obstacle", 3);// input -- obstacle
        shaderFluidAdvect.setFloat("Dissipation", VelocityDissipation);
        shaderFluidAdvect.setFloat("timeStep", TimeStep);

        glMemoryBarrier(GL_ALL_BARRIER_BITS);
        glDispatchCompute(tex_w/ WORKGROUP_SIZE, tex_h/ WORKGROUP_SIZE, 1); 
        Velocity.swap();

        
        // temperature advection
        shaderFluidAdvect.use();
        bindTexture_image(Temperature.pong_, 0);// output -- temperature pong
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, Velocity.ping_);
        shaderFluidAdvect.setInt("velocityPing", 1);// input -- velocity ping 
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, Temperature.ping_);
        shaderFluidAdvect.setInt("source", 2);// input -- source ping 
        glActiveTexture(GL_TEXTURE3);
        glBindTexture(GL_TEXTURE_2D, Obstacle);
        shaderFluidAdvect.setInt("obstacle", 3);// input -- obstacle
        shaderFluidAdvect.setFloat("Dissipation", TemperatureDissipation);
        glMemoryBarrier(GL_ALL_BARRIER_BITS);
        glDispatchCompute(tex_w / WORKGROUP_SIZE, tex_h / WORKGROUP_SIZE, 1);
        Temperature.swap();

        // density advection 
        shaderFluidAdvect.use();
        bindTexture_image(Density.pong_, 0);// output -- Density pong
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, Velocity.ping_);
        shaderFluidAdvect.setInt("velocityPing", 1);// input -- velocity ping 
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, Density.ping_);
        shaderFluidAdvect.setInt("source", 2);// input -- source ping 
        glActiveTexture(GL_TEXTURE3);
        glBindTexture(GL_TEXTURE_2D, Obstacle);
        shaderFluidAdvect.setInt("obstacle", 3);// input -- obstacle
        shaderFluidAdvect.setFloat("Dissipation", DensityDissipation);
        glMemoryBarrier(GL_ALL_BARRIER_BITS);
        glDispatchCompute(tex_w / WORKGROUP_SIZE, tex_h / WORKGROUP_SIZE, 1);
        Density.swap();

        // vorticity confinement
        shaderFluidVorticity.use();
        bindTexture_image(Velocity.pong_, 0);// output -- Velocity pong
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, Velocity.ping_);
        shaderFluidVorticity.setInt("velocityPing", 1);// input -- velocity ping 
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, Obstacle);
        shaderFluidVorticity.setInt("obstacle", 2);// input -- obstacle
        shaderFluidVorticity.setFloat("cellSize", CellSize);
        shaderFluidVorticity.setFloat("timeStep", TimeStep);
        glMemoryBarrier(GL_ALL_BARRIER_BITS);
        glDispatchCompute(tex_w / WORKGROUP_SIZE, tex_h / WORKGROUP_SIZE, 1);
        Velocity.swap();

        // apply buoyancy
        shaderFluidApplyBuo.use();
        bindTexture_image(Velocity.pong_, 0);// output -- velocity pong
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, Velocity.ping_);
        shaderFluidApplyBuo.setInt("velocityPing", 1);// input -- velocity ping 
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, Temperature.ping_);
        shaderFluidApplyBuo.setInt("temperaturePing", 2);// input -- Temperature ping 
        glActiveTexture(GL_TEXTURE3);
        glBindTexture(GL_TEXTURE_2D, Density.ping_);
        shaderFluidApplyBuo.setInt("densityPing", 3);// input -- Density ping 
        shaderFluidApplyBuo.setFloat("timeStep", TimeStep);
        glMemoryBarrier(GL_ALL_BARRIER_BITS);
        glDispatchCompute(tex_w / WORKGROUP_SIZE, tex_h / WORKGROUP_SIZE, 1);
        Velocity.swap();

        // apply impulse temperature
        shaderFluidApplyImp.use();
        bindTexture_image(Temperature.pong_, 0);// output -- Temperature ping
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, Temperature.ping_);// input temperature
        shaderFluidApplyImp.setInt("input", 1);
        shaderFluidApplyImp.setVec2("impulsePosition", ImpulseLocation);
        shaderFluidApplyImp.setFloat("fillColor", ImpulseTemperature);
        shaderFluidApplyImp.setFloat("timeStep", TimeStep);
        glMemoryBarrier(GL_ALL_BARRIER_BITS);
        glDispatchCompute(tex_w / WORKGROUP_SIZE, tex_h / WORKGROUP_SIZE, 1);
        Temperature.swap();
        

        // apply impulse density
        shaderFluidApplyImp.use();
        bindTexture_image(Density.pong_, 0);// output -- Density ping
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, Density.ping_);// input density
        shaderFluidApplyImp.setInt("input", 0);
        shaderFluidApplyImp.setVec2("impulsePosition", ImpulseLocation);
        shaderFluidApplyImp.setFloat("fillColor", ImpulseDensity);
        glMemoryBarrier(GL_ALL_BARRIER_BITS);
        glDispatchCompute(tex_w / WORKGROUP_SIZE, tex_h / WORKGROUP_SIZE, 1);
        Density.swap();

        // push fluid
        shaderFluidPush.use();
        bindTexture_image(Velocity.pong_, 0);// output -- velocity pong
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, Velocity.ping_);// input velocity
        shaderFluidPush.setInt("velocity", 0);
        shaderFluidPush.setVec2("m_pos", m_pos);
        shaderFluidPush.setVec2("m_delta", m_delta);
        shaderFluidPush.setBool("m_pressed", m_pressed);
        glMemoryBarrier(GL_ALL_BARRIER_BITS);
        glDispatchCompute(tex_w / WORKGROUP_SIZE, tex_h / WORKGROUP_SIZE, 1);
        Velocity.swap();

        // compute divergence 
        shaderFluidCmpDiver.use();
        bindTexture_image(Divergence, 0);// output -- Divergence
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, Velocity.ping_);
        shaderFluidCmpDiver.setInt("velocityPing", 1);// input -- obstacle
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, Obstacle);
        shaderFluidCmpDiver.setInt("obstacle", 2);// input -- obstacle
        shaderFluidCmpDiver.setFloat("timeStep", TimeStep);
        shaderFluidCmpDiver.setFloat("cellSize", CellSize);
        glMemoryBarrier(GL_ALL_BARRIER_BITS);
        glDispatchCompute(tex_w / WORKGROUP_SIZE, tex_h / WORKGROUP_SIZE, 1);
        

        // clear pressure
        shaderFluidClear.use();
        glBindImageTexture(0, Pressure.ping_, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA32F);// output -- pressure ping
        glMemoryBarrier(GL_ALL_BARRIER_BITS);
        glDispatchCompute(tex_w / WORKGROUP_SIZE, tex_h / WORKGROUP_SIZE, 1);
        

        // jacobi iteration

        for (int i = 0; i < NumJacobiIterations; ++i) {
            shaderFluidJacobi.use();
            glBindImageTexture(0, Pressure.pong_, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA32F);// output -- pressure pong
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, Pressure.ping_);
            shaderFluidJacobi.setInt("pressurePing", 1);// input -- pressure ping
            glActiveTexture(GL_TEXTURE2);
            glBindTexture(GL_TEXTURE_2D, Obstacle);
            shaderFluidJacobi.setInt("obstacle", 2);// input -- obstacle   
            glActiveTexture(GL_TEXTURE3);
            glBindTexture(GL_TEXTURE_2D, Divergence);
            shaderFluidJacobi.setInt("divergence", 3);// input -- obstacle   
            shaderFluidJacobi.setFloat("cellSize", CellSize);
            glMemoryBarrier(GL_ALL_BARRIER_BITS);
            glDispatchCompute(tex_w / WORKGROUP_SIZE, tex_h / WORKGROUP_SIZE, 1);
            Pressure.swap();
        }

        // subtract gradient
        shaderFluidSubtractGrad.use();
        glBindImageTexture(0, Velocity.pong_, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA32F);// output -- velocity pong
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, Velocity.ping_);
        shaderFluidSubtractGrad.setInt("velocityPing", 1);// input -- velocity ping
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, Pressure.ping_);
        shaderFluidSubtractGrad.setInt("pressurePing", 2);// input -- pressure ping
        glActiveTexture(GL_TEXTURE3);
        glBindTexture(GL_TEXTURE_2D, Obstacle);
        shaderFluidSubtractGrad.setInt("obstacle", 3);// input -- obstacle
        shaderFluidSubtractGrad.setFloat("timeStep", TimeStep);
        shaderFluidSubtractGrad.setFloat("cellSize", CellSize);
        glMemoryBarrier(GL_ALL_BARRIER_BITS);
        glDispatchCompute(tex_w / WORKGROUP_SIZE, tex_h / WORKGROUP_SIZE, 1);
        Velocity.swap();


        // render
        // ------
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

      
        
#if Render_rs
        //last one follow realsense
        glm::vec3 _rs_pos = glm::vec3(pose_data.translation.x, pose_data.translation.y,
            pose_data.translation.z);
        auto _rs_rot = pose_data.rotation;
        float _m_scale = 3.0f;
        model = glm::mat4(1.0f);
        glm::quat quat1 = glm::quat(_rs_rot.w, _rs_rot.x, _rs_rot.y, _rs_rot.z);
        glm::mat4 model_rot = glm::toMat4(quat1);
        glm::mat4 model_scale = glm::scale(glm::mat4(1.0), glm::vec3(0.125));
        glm::mat4 model_translate = glm::translate(glm::mat4(1.0), _rs_pos * _m_scale);
        model =  model_translate *model_rot * model_scale * model; /// M = T * R * S * v
        shaderGeometryPass.setMat4("model", model);
        my_cube.renderCube();
#endif

        glBindFramebuffer(GL_FRAMEBUFFER, 0);

     

        // ----visulize fluid here---
        glViewport(0, 0, SCR_WIDTH, SCR_WIDTH);
        shaderCustomBuffer.use();
        shaderCustomBuffer.setInt("density", 0);
        shaderCustomBuffer.setInt("obstacle", 1);
        shaderCustomBuffer.setInt("divergence", 2);
        shaderCustomBuffer.setInt("pressure", 3);
        shaderCustomBuffer.setInt("velocity", 4);
        glActiveTexture(GL_TEXTURE4);
        glBindTexture(GL_TEXTURE_2D, Velocity.ping_);
        glActiveTexture(GL_TEXTURE3);
        glBindTexture(GL_TEXTURE_2D, Pressure.ping_);
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, Divergence);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, Obstacle);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, Density.ping_);
        shaderCustomBuffer.setVec3("viewPos", camera.Position);
        my_quad.renderQuad(dy_factor);
        // glfw: swap buffers and poll IO events (keys pressed/released, mouse moved etc.)
        // -------------------------------------------------------------------------------
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

 
    my_quad.destroyQuad();
    glfwTerminate();
    //_CrtDumpMemoryLeaks();
    return 0;
}

// process all input: query GLFW whether relevant keys are pressed/released this frame and react accordingly
// ---------------------------------------------------------------------------------------------------------
void processInput(GLFWwindow *window)
{
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        camera.ProcessKeyboard(FORWARD, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        camera.ProcessKeyboard(BACKWARD, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        camera.ProcessKeyboard(LEFT, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        camera.ProcessKeyboard(RIGHT, deltaTime);
}

// glfw: whenever the window size changed (by OS or user resize) this callback function executes
// ---------------------------------------------------------------------------------------------
void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    // make sure the viewport matches the new window dimensions; note that width and 
    // height will be significantly larger than specified on retina displays.
    glViewport(0, 0, width, height);
}

// glfw: whenever the mouse moves, this callback is called
// -------------------------------------------------------
void mouse_callback(GLFWwindow* window, double xpos, double ypos)
{
    if (firstMouse)
    {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }

    float xoffset = xpos - lastX;
    float yoffset = lastY - ypos; // reversed since y-coordinates go from bottom to top

    lastX = xpos;
    lastY = ypos;

    float maximum_vel = 3.0f;
    float mouse_vel = sqrt(xoffset * xoffset + yoffset * yoffset);
    m_pos = glm::vec2(xpos, ypos);
    m_delta = glm::vec2(xoffset, yoffset);
    //dy_factor = std::min(maximum_vel/mouse_vel, 1.0f);
    camera.ProcessMouseMovement(xoffset, yoffset);
}

void mouse_button_callback(GLFWwindow* window, int button, int action, int mods) {
    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS)
        m_pressed = true;
    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_RELEASE)
        m_pressed = false;
}

// glfw: whenever the mouse scroll wheel scrolls, this callback is called
// ----------------------------------------------------------------------
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
    camera.ProcessMouseScroll(yoffset);
}

unsigned int TextureFromFile(const string path, bool gamma )
{

    unsigned int textureID;
    glGenTextures(1, &textureID);

    int width, height, nrComponents;
    unsigned char* data = stbi_load(path.c_str(), &width, &height, &nrComponents, 0);
    if (data)
    {
        GLenum format;
        if (nrComponents == 1)
            format = GL_RED;
        else if (nrComponents == 3)
            format = GL_RGB;
        else if (nrComponents == 4)
            format = GL_RGBA;

        glBindTexture(GL_TEXTURE_2D, textureID);
        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        stbi_image_free(data);
    }
    else
    {
        std::cout << "Texture failed to load at path: " << path << std::endl;
        stbi_image_free(data);
    }

    return textureID;
}