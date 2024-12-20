/**
* Author: Jason Wu
* Assignment: Lunar Lander
* Date due: 2024-10-26, 11:59pm
* I pledge that I have completed this assignment without
* collaborating with anyone else, in conformance with the
* NYU School of Engineering Policies and Procedures on
* Academic Misconduct.
**/

#define LOG(argument) std::cout << argument << '\n'
#define STB_IMAGE_IMPLEMENTATION
#define GL_SILENCE_DEPRECATION
#define GL_GLEXT_PROTOTYPES 1

#ifdef _WINDOWS
    #include <GL/glew.h>
#endif

#include <SDL.h>
#include <SDL_opengl.h>
#include "glm/mat4x4.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "ShaderProgram.h"
#include "stb_image.h"
#include "cmath"
#include <ctime>
#include <cstdlib>  // For rand() and srand()
#include <vector>
#include "Entity.h"
#include <string.h>

// ————— CONSTANTS ————— //
constexpr int WINDOW_WIDTH  = 640 * 2,
              WINDOW_HEIGHT = 800;

constexpr float BG_RED = 0.1922f,
                BG_BLUE = 0.549f,
                BG_GREEN = 0.9059f,
                BG_OPACITY = 1.0f;

constexpr int VIEWPORT_X = 0,
              VIEWPORT_Y = 0,
              VIEWPORT_WIDTH  = WINDOW_WIDTH,
              VIEWPORT_HEIGHT = WINDOW_HEIGHT;

constexpr char V_SHADER_PATH[] = "shaders/vertex_textured.glsl",
               F_SHADER_PATH[] = "shaders/fragment_textured.glsl";

constexpr float MILLISECONDS_IN_SECOND = 1000.0;
constexpr char  EXPLOSION_FILEPATH[] = "Explosion.png",
                ASTEROIDS_FILEPATH[] = "Asteroids.png",
                FONTSHEET_FILEPATH[]   = "font1.png",
                PLATFORM_FILEPATH[]    = "world_tileset.png",
                SPACESHIP_FILEPATH[]   = "Spaceships.png";

constexpr GLint NUMBER_OF_TEXTURES = 1;
constexpr GLint LEVEL_OF_DETAIL    = 0;
constexpr GLint TEXTURE_BORDER     = 0;

constexpr float FIXED_TIMESTEP = 1.0f / 60.0f;
constexpr float ACC_OF_GRAVITY = -9.81f;
constexpr int   PLATFORM_COUNT = 20;
constexpr int   ASTEROID_COUNT = 5;


// ————— STRUCTS AND ENUMS —————//
enum AppStatus { RUNNING, TERMINATED };

struct GameState
{
    Entity* player;
    Entity* collidables;
    Entity* others;
};

// ————— VARIABLES ————— //
GameState g_game_state;

SDL_Window* g_display_window;
AppStatus g_app_status = RUNNING;

ShaderProgram g_shader_program = ShaderProgram();
glm::mat4 g_view_matrix, g_projection_matrix;

float g_previous_ticks   = 0.0f;
float g_time_accumulator = 0.0f;
bool isRunning = false;
float fuel = 100;
constexpr int FONTBANK_SIZE = 16;
GLuint g_font_texture_id;
int gameMessage = 0;
int gameStat = 0;

// ———— GENERAL FUNCTIONS ———— //
GLuint load_texture(const char* filepath);
void draw_sprite_from_texture_atlas(ShaderProgram *program, GLuint texture_id, int index,
                                    int rows, int cols);

void initialise();
void process_input();
void update();
void render();
void shutdown();

GLuint load_texture(const char* filepath)
{
    int width, height, number_of_components;
    unsigned char* image = stbi_load(filepath, &width, &height, &number_of_components,
                                     STBI_rgb_alpha);

    if (image == NULL)
    {
        LOG("Unable to load image. Make sure the path is correct.");
        assert(false);
    }

    GLuint textureID;
    glGenTextures(NUMBER_OF_TEXTURES, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);
    glTexImage2D(GL_TEXTURE_2D, LEVEL_OF_DETAIL, GL_RGBA, width, height, TEXTURE_BORDER,
                 GL_RGBA, GL_UNSIGNED_BYTE, image);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    stbi_image_free(image);

    return textureID;
}

void draw_text(ShaderProgram *shader_program, GLuint font_texture_id, std::string text,
               float font_size, float spacing, glm::vec3 position)
{
    // Scale the size of the fontbank in the UV-plane
    // We will use this for spacing and positioning
    float width = 1.0f / FONTBANK_SIZE;
    float height = 1.0f / FONTBANK_SIZE;

    // Instead of having a single pair of arrays, we'll have a series of pairs—one for
    // each character. Don't forget to include <vector>!
    std::vector<float> vertices;
    std::vector<float> texture_coordinates;

    // For every character...
    for (int i = 0; i < text.size(); i++) {
        // 1. Get their index in the spritesheet, as well as their offset (i.e. their
        //    position relative to the whole sentence)
        int spritesheet_index = (int) text[i];  // ascii value of character
        float offset = (font_size + spacing) * i;

        // 2. Using the spritesheet index, we can calculate our U- and V-coordinates
        float u_coordinate = (float) (spritesheet_index % FONTBANK_SIZE) / FONTBANK_SIZE;
        float v_coordinate = (float) (spritesheet_index / FONTBANK_SIZE) / FONTBANK_SIZE;

        // 3. Inset the current pair in both vectors
        vertices.insert(vertices.end(), {
            offset + (-0.5f * font_size), 0.5f * font_size,
            offset + (-0.5f * font_size), -0.5f * font_size,
            offset + (0.5f * font_size), 0.5f * font_size,
            offset + (0.5f * font_size), -0.5f * font_size,
            offset + (0.5f * font_size), 0.5f * font_size,
            offset + (-0.5f * font_size), -0.5f * font_size,
        });

        texture_coordinates.insert(texture_coordinates.end(), {
            u_coordinate, v_coordinate,
            u_coordinate, v_coordinate + height,
            u_coordinate + width, v_coordinate,
            u_coordinate + width, v_coordinate + height,
            u_coordinate + width, v_coordinate,
            u_coordinate, v_coordinate + height,
        });
    }

    // 4. And render all of them using the pairs
    glm::mat4 model_matrix = glm::mat4(1.0f);
    model_matrix = glm::translate(model_matrix, position);

    shader_program->set_model_matrix(model_matrix);
    glUseProgram(shader_program->get_program_id());

    glVertexAttribPointer(shader_program->get_position_attribute(), 2, GL_FLOAT, false, 0,
                          vertices.data());
    glEnableVertexAttribArray(shader_program->get_position_attribute());

    glVertexAttribPointer(shader_program->get_tex_coordinate_attribute(), 2, GL_FLOAT,
                          false, 0, texture_coordinates.data());
    glEnableVertexAttribArray(shader_program->get_tex_coordinate_attribute());

    glBindTexture(GL_TEXTURE_2D, font_texture_id);
    glDrawArrays(GL_TRIANGLES, 0, (int) (text.size() * 6));

    glDisableVertexAttribArray(shader_program->get_position_attribute());
    glDisableVertexAttribArray(shader_program->get_tex_coordinate_attribute());
}

void initialise()
{
    SDL_Init(SDL_INIT_VIDEO);
    g_display_window = SDL_CreateWindow("Project 3",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        WINDOW_WIDTH, WINDOW_HEIGHT,
        SDL_WINDOW_OPENGL);

    SDL_GLContext context = SDL_GL_CreateContext(g_display_window);
    SDL_GL_MakeCurrent(g_display_window, context);
    
    if (context == nullptr)
    {
        LOG("ERROR: Could not create OpenGL context.\n");
        shutdown();
    }

#ifdef _WINDOWS
    glewInit();
#endif

    glViewport(VIEWPORT_X, VIEWPORT_Y, VIEWPORT_WIDTH, VIEWPORT_HEIGHT);

    g_shader_program.load(V_SHADER_PATH, F_SHADER_PATH);

    g_view_matrix       = glm::mat4(1.0f);
    g_projection_matrix = glm::ortho(-5.0f, 5.0f, -3.75f, 3.75f, -1.0f, 1.0f);

    g_shader_program.set_projection_matrix(g_projection_matrix);
    g_shader_program.set_view_matrix(g_view_matrix);

    glUseProgram(g_shader_program.get_program_id());

    glClearColor(BG_RED, BG_BLUE, BG_GREEN, BG_OPACITY);

    // ————— PLAYER ————— //
    GLuint player_texture_id = load_texture(SPACESHIP_FILEPATH);

    g_game_state.player = new Entity(
        player_texture_id,         // texture id
        1.0f,                      // speed
        9,                         // current animation index
        5,                         // animation column amount
        3                          // animation row amount
    );

    g_game_state.player->face_up();
    g_game_state.player->set_position(glm::vec3(0.0f, 2.9f, 0.0f)); // Start at top of screen
    g_game_state.player->set_width(g_game_state.player->get_width() * 0.5f);
    g_game_state.player->set_acceleration(glm::vec3(0.0f, ACC_OF_GRAVITY * 0.005, 0.0f));
    g_game_state.player->update(0.0f, nullptr, 0);

    // ————— COLLIDABLES ————— //
    // Allocate memory for collidables
    g_game_state.collidables = new Entity[PLATFORM_COUNT + ASTEROID_COUNT];
    

    // Seed the random number generator
    std::srand(static_cast<unsigned>(std::time(nullptr)));

    // Load textures only once
    GLuint platform_texture_id = load_texture(PLATFORM_FILEPATH);
    GLuint asteroid_texture_id = load_texture(ASTEROIDS_FILEPATH);

    // Generate a random index for a special platform
    int randomInt = std::rand() % PLATFORM_COUNT;

    for (int i = 0; i < PLATFORM_COUNT + ASTEROID_COUNT; i++) {
        if (i < PLATFORM_COUNT){
            if (i == randomInt) {
                g_game_state.collidables[i] = Entity(platform_texture_id, 0.0f, 0, 16, 16);
                g_game_state.collidables[i].set_landingStatus(true);  // Special landing platform
            } else {
                g_game_state.collidables[i] = Entity(platform_texture_id, 0.0f, 5, 16, 16);
                g_game_state.collidables[i].set_landingStatus(false); // Regular platform
            }
            
            g_game_state.collidables[i].face_right();
            g_game_state.collidables[i].set_position(glm::vec3(-4.75f + (i * 0.5f), -3.5f, 0.0f));
            g_game_state.collidables[i].update(0.0f, nullptr, 0);
        }
        else {
            float randomX = -4.0f + static_cast<float>(std::rand()) / (static_cast<float>(RAND_MAX / 8.0f)); // Range -4.0 to 4.0
            float randomY = static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX) * 3.0f - 1.0f; // Range -1.0 to 2.0
            g_game_state.collidables[i] = Entity(asteroid_texture_id, 0.0f, 0, 4, 1);
            g_game_state.collidables[i].set_position(glm::vec3(randomX, randomY, 0.0f));
            g_game_state.collidables[i].set_landingStatus(false);
        }
        g_game_state.collidables[i].set_scale(glm::vec3(0.5f, 0.5f, 0.0f));
        //Drag the height and width closer to rendered entity
        g_game_state.collidables[i].set_width(g_game_state.collidables[i].get_width() * 0.1f);
        g_game_state.collidables[i].set_height(g_game_state.collidables[i].get_height() * 0.1f);
        g_game_state.collidables[i].update(0.0f, nullptr, 0);
    }
    
    
    // ————— OTHERS ————— //
    std::string first = "health_0";
    std::string ext = ".png";
    
    g_game_state.others = new Entity[10];
    for (int i = 0; i < 10; i++)
    {
        std::string curr = first + std::to_string(i) + ext;
        GLuint fuel_texture_id = load_texture(curr.c_str());
        g_game_state.others[i] = Entity(fuel_texture_id, 0.0f, 1, 1, 1);
        g_game_state.others[i].set_position(glm::vec3(4.5f, 3.5f, 0.0f));
        g_game_state.others[i].set_scale(glm::vec3(0.5f, 0.25f, 0.0f));
        g_game_state.others[i].face_right();
        g_game_state.others[i].update(0.0f, nullptr, 0);
    }

    



    // ————— GENERAL ————— //
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void process_input()
{
    // VERY IMPORTANT: If nothing is pressed, we don't want to go anywhere
    g_game_state.player->set_movement(glm::vec3(0.0f));

    SDL_Event event;
    while (SDL_PollEvent(&event))
    {
        switch (event.type) {
            // End game
        case SDL_QUIT:
        case SDL_WINDOWEVENT_CLOSE:
            g_app_status = TERMINATED;
            break;

        case SDL_KEYDOWN:
            switch (event.key.keysym.sym) {
            case SDLK_q:
                // Quit the game with a keystroke
                g_app_status = TERMINATED;
                break;
            case SDLK_SPACE:
                isRunning = true;

            default:
                break;
            }

        default:
            break;
        }
    }

    const Uint8* key_state = SDL_GetKeyboardState(NULL);
    
    if (isRunning) {
        if (key_state[SDL_SCANCODE_LEFT]){
            if (fuel > 0) {
                g_game_state.player->move_left();
                fuel -= 0.3;
            }
        }
        else if (key_state[SDL_SCANCODE_RIGHT]) {
            if (fuel > 0) {
                g_game_state.player->move_right();
                fuel -= 0.3;
            }
        
        }
        else {
            g_game_state.player->face_up();
        }
        
        // This makes sure that the player can't move faster diagonally
        if (glm::length(g_game_state.player->get_movement()) > 1.0f)
            g_game_state.player->normalise_movement();
    }
}

void update()
{
    // ————— DELTA TIME ————— //
    float ticks = (float)SDL_GetTicks() / MILLISECONDS_IN_SECOND;
    float delta_time = ticks - g_previous_ticks;
    g_previous_ticks = ticks;

    // ————— FIXED TIMESTEP ————— //
    // STEP 1: Keep track of how much time has passed since last step
    delta_time += g_time_accumulator;

    // STEP 2: Accumulate the ammount of time passed while we're under our fixed timestep
    if (delta_time < FIXED_TIMESTEP)
    {
        g_time_accumulator = delta_time;
        return;
    }

    // STEP 3: Once we exceed our fixed timestep, apply that elapsed time into the
    //         objects' update function invocation
    while (delta_time >= FIXED_TIMESTEP)
    {
        // Notice that we're using FIXED_TIMESTEP as our delta time
        if(isRunning){
            g_font_texture_id = load_texture(FONTSHEET_FILEPATH);
            int gameStatus = g_game_state.player->update(FIXED_TIMESTEP, g_game_state.collidables,
                                                         PLATFORM_COUNT + ASTEROID_COUNT);
            if(gameStatus == 1) {
                gameMessage = 1;
                gameStat = 1;
                isRunning = false;
            }
            else if (gameStatus == 2) {
                gameMessage = 2;
                gameStat = 2;
                isRunning = false;
            }
            if (g_game_state.player->get_position().x > 5.0f || g_game_state.player->get_position().x < -5.0f || gameStatus == 3) {
                glm::vec3 curr_pos = g_game_state.player->get_position();
                GLuint explosion_texture_id = load_texture(EXPLOSION_FILEPATH);
                g_game_state.player = nullptr;
                g_game_state.player = new Entity(
                     explosion_texture_id,
                     0.0f,
                     1,
                     8,
                     1
                );
                g_game_state.player->set_position(curr_pos);
                g_game_state.player->update(0.0f, nullptr, 0);
                isRunning = false;
                gameMessage = 2;
                gameStat = 3;
            }
        }
        delta_time -= FIXED_TIMESTEP;
    }

    g_time_accumulator = delta_time;
}

void render()
{
    // ————— GENERAL ————— //
    glClear(GL_COLOR_BUFFER_BIT);

    // ————— PLAYER ————— //
    g_game_state.player->render(&g_shader_program);

    // ————— COLLIDABLES ————— //
    for (int i = 0; i < PLATFORM_COUNT + ASTEROID_COUNT; i++)
        g_game_state.collidables[i].render(&g_shader_program);
    
    // ————— OTHERS ————— //
    if (!isRunning) {
        if(gameStat != 1 && gameStat != 2 && gameStat != 3) {
            char INITHEALTH_FILEPATH[]   = "health_10.png";
            GLuint init_health_texture_id = load_texture(INITHEALTH_FILEPATH);
            Entity init_health = Entity(init_health_texture_id, 0.0f, 1, 1, 1);
            init_health.set_position(glm::vec3(4.5f, 3.5f, 0.0f));
            init_health.set_scale(glm::vec3(0.5f, 0.25f, 0.0f));
            init_health.face_right();
            init_health.update(0.0f, nullptr, 0);
            init_health.render(&g_shader_program);
        }
    }
    else {
        if (static_cast<int>(std::round(fuel / 10.0f) == 0)) {
            g_game_state.others[static_cast<int>(std::round(fuel / 10.0f))].render(&g_shader_program);
        }
        else {
            g_game_state.others[static_cast<int>(std::round(fuel / 10.0f)) - 1].render(&g_shader_program);
        }
    }
    
    
    if(!isRunning && gameMessage != 0) {
        if(gameMessage == 1)
        {
            draw_text(&g_shader_program, g_font_texture_id, "MISSION SUCCESS", 0.5f, 0.05f,
                      glm::vec3(-3.5f, 2.5f, 0.0f));
        }
        else if(gameMessage == 2) {
            draw_text(&g_shader_program, g_font_texture_id, "MISSION FAIL", 0.5f, 0.05f,
                      glm::vec3(-2.5f, 2.5f, 0.0f));
        }
    }
    // ————— GENERAL ————— //
    SDL_GL_SwapWindow(g_display_window);
}

void shutdown()
{
    SDL_Quit();
    
    delete   g_game_state.player;
    delete[] g_game_state.collidables;
    delete[] g_game_state.others;
}


int main(int argc, char* argv[])
{
    initialise();

    while (g_app_status == RUNNING)
    {
        process_input();
        update();
        render();
    }

    shutdown();
    return 0;
}
