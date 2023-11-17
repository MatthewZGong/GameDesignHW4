/**
* Author: Matthew Gong
* Assignment: Rise of the AI
* Date due: 2023-11-18, 11:59pm
* I pledge that I have completed this assignment without
* collaborating with anyone else, in conformance with the
* NYU School of Engineering Policies and Procedures on
* Academic Misconduct.
**/

#define GL_SILENCE_DEPRECATION
#define STB_IMAGE_IMPLEMENTATION
#define LOG(argument) std::cout << argument << '\n'
#define GL_GLEXT_PROTOTYPES 1
#define FIXED_TIMESTEP 0.0166666f
#define PLATFORM_COUNT 11
#define ENEMY_COUNT 3

#ifdef _WINDOWS
#include <GL/glew.h>
#endif

#include <SDL.h>
#include <SDL_opengl.h>
#include <SDL_mixer.h>
#include "glm/mat4x4.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "ShaderProgram.h"
#include "stb_image.h"
#include "cmath"
#include <ctime>
#include <vector>
#include "Entity.h"
#include "Map.h"

// ––––– STRUCTS AND ENUMS ––––– //
struct GameState
{
    Entity* player;
    Entity* enemies;
    Map* map;
    Mix_Music* bgm;
    Mix_Chunk* jump_sfx;
    int state = 0;
};

// ––––– CONSTANTS ––––– //
const int   WINDOW_WIDTH = 640,
            WINDOW_HEIGHT = 480;

const float BG_RED = 0.1922f,
            BG_BLUE = 0.549f,
            BG_GREEN = 0.9059f,
            BG_OPACITY = 1.0f;

const int   VIEWPORT_X = 0,
            VIEWPORT_Y = 0,
            VIEWPORT_WIDTH = WINDOW_WIDTH,
            VIEWPORT_HEIGHT = WINDOW_HEIGHT;

const char  V_SHADER_PATH[] = "shaders/vertex_textured.glsl",
            F_SHADER_PATH[] = "shaders/fragment_textured.glsl";

const float MILLISECONDS_IN_SECOND  = 1000.0;
const char  SPRITE_TILESET_FILEPATH[] = "resources/tilemap-characters_packed.png",
            MAP_TILESET_FILEPATH[]  = "resources/tilemap_packed.png",
            FONTSHEET_FILEPATH[] = "resources/font1.png";
            



const char  BGM_FILEPATH[]          = "resources/adventure.mp3",
            BOUNCING_SFX_FILEPATH[] = "resources/jump.wav";
const int   LOOP_FOREVER     = -1;  // -1 means loop forever in Mix_PlayMusic; 0 means play once and loop zero times

const int NUMBER_OF_TEXTURES = 1;
const GLint LEVEL_OF_DETAIL  = 0;
const GLint TEXTURE_BORDER   = 0;

// BGM
const int   CD_QUAL_FREQ    = 44100,  // CD quality
            AUDIO_CHAN_AMT  = 2,      // Stereo
            AUDIO_BUFF_SIZE = 4096;

// SFX
const int   PLAY_ONCE   = 0,
            NEXT_CHNL   = -1,  // next available channel
            MUTE_VOL    = 0,
            MILS_IN_SEC = 1000,
            ALL_SFX_CHN = -1;
//Map Data

const int LEVEL1_HEIGHT = 5;
const int LEVEL1_WIDTH = 24;
int E = -1;
int LEVEL_1_DATA[] =
{
    E, E, E, E, E, E, E, E,                        E, E, E, E,E,E, E, E, E, E, E, E, 1, 2, 2, 3,
    E, E, E, E, E, E, E, E,                        1, 2, 2, 3,E,E, E, E, E, E, E, E, E, E, E, E,
    21, 23, E,  E,E, E, E, E,                      E, E, E,E,E, E, E, E, 21, 22, 22, 23,E,E, E, E,
    4, 4, 22, 22, 22, 22, 22, 23,                      E, E,E,E, 21, 22, 22, 22, 4, 4, 4, 4, E, E,E, E,
    4, 4, 4, 4, 4, 4, 4, 4,                         E, E,E,E, 4, 4, 4, 4, 4, 4, 4, 4, E, E,E, E,
};



// ––––– GLOBAL VARIABLES ––––– //
GameState g_game_state;

SDL_Window* g_display_window;
bool g_game_is_running = true;

ShaderProgram g_shader_program;
glm::mat4 g_view_matrix, g_projection_matrix;

float g_previous_ticks = 0.0f;
float g_time_accumulator = 0.0f;

// Audio
Mix_Music* g_music;
Mix_Chunk* g_bouncing_sfx;

const int FONTBANK_SIZE = 16;
GLuint FONT_TEXTURE_ID;

void DrawText(ShaderProgram *program, GLuint font_texture_id, std::string text, float screen_size, float spacing, glm::vec3 position)
{
    // Scale the size of the fontbank in the UV-plane
    // We will use this for spacing and positioning
    float width = 1.0f / FONTBANK_SIZE;
    float height = 1.0f / FONTBANK_SIZE;

    // Instead of having a single pair of arrays, we'll have a series of pairs—one for each character
    // Don't forget to include <vector>!
    std::vector<float> vertices;
    std::vector<float> texture_coordinates;

    // For every character...
    for (int i = 0; i < text.size(); i++) {
        // 1. Get their index in the spritesheet, as well as their offset (i.e. their position
        //    relative to the whole sentence)
        int spritesheet_index = (int) text[i];  // ascii value of character
        float offset = (screen_size + spacing) * i;
        
        // 2. Using the spritesheet index, we can calculate our U- and V-coordinates
        float u_coordinate = (float) (spritesheet_index % FONTBANK_SIZE) / FONTBANK_SIZE;
        float v_coordinate = (float) (spritesheet_index / FONTBANK_SIZE) / FONTBANK_SIZE;

        // 3. Inset the current pair in both vectors
        vertices.insert(vertices.end(), {
            offset + (-0.5f * screen_size), 0.5f * screen_size,
            offset + (-0.5f * screen_size), -0.5f * screen_size,
            offset + (0.5f * screen_size), 0.5f * screen_size,
            offset + (0.5f * screen_size), -0.5f * screen_size,
            offset + (0.5f * screen_size), 0.5f * screen_size,
            offset + (-0.5f * screen_size), -0.5f * screen_size,
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
    
    program->SetModelMatrix(model_matrix);
    glUseProgram(program->programID);
    
    glVertexAttribPointer(program->positionAttribute, 2, GL_FLOAT, false, 0, vertices.data());
    glEnableVertexAttribArray(program->positionAttribute);
    glVertexAttribPointer(program->texCoordAttribute, 2, GL_FLOAT, false, 0, texture_coordinates.data());
    glEnableVertexAttribArray(program->texCoordAttribute);
    
    glBindTexture(GL_TEXTURE_2D, font_texture_id);
    glDrawArrays(GL_TRIANGLES, 0, (int) (text.size() * 6));
    
    glDisableVertexAttribArray(program->positionAttribute);
    glDisableVertexAttribArray(program->texCoordAttribute);
}


void lose(){
    glm::vec3 location = g_game_state.player->get_position();
    location.y = 2.5;
    location.x -= 4.5;
    DrawText(&g_shader_program, FONT_TEXTURE_ID, "YOU LOSE", 1.0f, 0.0001f, location);
//    std::cout << "YOU LOST" << std::endl;
}

void win(){
    glm::vec3 location = g_game_state.player->get_position();
    location.y = 2.5;
    location.x -= 4.5;
    DrawText(&g_shader_program, FONT_TEXTURE_ID, "YOU WIN!", 1.0f, 0.0001f, location);
//    std::cout << "YOU WON" << std::endl;
}

// ———— GENERAL FUNCTIONS ———— //
GLuint load_texture(const char* filepath)
{
    int width, height, number_of_components;
    unsigned char* image = stbi_load(filepath, &width, &height, &number_of_components, STBI_rgb_alpha);

    if (image == NULL)
    {
        LOG("Unable to load image. Make sure the path is correct.");
        assert(false);
    }

    GLuint textureID;
    glGenTextures(NUMBER_OF_TEXTURES, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);
    glTexImage2D(GL_TEXTURE_2D, LEVEL_OF_DETAIL, GL_RGBA, width, height, TEXTURE_BORDER, GL_RGBA, GL_UNSIGNED_BYTE, image);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    stbi_image_free(image);

    return textureID;
}


void initialise()
{
    // Initialising both the video AND audio subsystems
    // We did something similar when we talked about video game controllers
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO);
    g_display_window = SDL_CreateWindow("Rise of the AI!",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        WINDOW_WIDTH, WINDOW_HEIGHT,
        SDL_WINDOW_OPENGL);

    SDL_GLContext context = SDL_GL_CreateContext(g_display_window);
    SDL_GL_MakeCurrent(g_display_window, context);

#ifdef _WINDOWS
    glewInit();
#endif
    // ––––– VIDEO SETUP ––––– //
    glViewport(VIEWPORT_X, VIEWPORT_Y, VIEWPORT_WIDTH, VIEWPORT_HEIGHT);

    g_shader_program.Load(V_SHADER_PATH, F_SHADER_PATH);

    g_view_matrix = glm::mat4(1.0f);
    g_projection_matrix = glm::ortho(-5.0f, 5.0f, -3.75f, 3.75f, -1.0f, 1.0f);

    g_shader_program.SetProjectionMatrix(g_projection_matrix);
    g_shader_program.SetViewMatrix(g_view_matrix);

    glUseProgram(g_shader_program.programID);

    glClearColor(BG_RED, BG_BLUE, BG_GREEN, BG_OPACITY);

  
    // ————— MAP SET-UP ————— //
    GLuint map_texture_id = load_texture(MAP_TILESET_FILEPATH);
    g_game_state.map = new Map(LEVEL1_WIDTH, LEVEL1_HEIGHT, LEVEL_1_DATA, map_texture_id, 1.0f, 20, 9);


    // ––––– PLAYER (GEORGE) ––––– //
    // Existing
    GLuint sprite_tileset_texture_id = load_texture(SPRITE_TILESET_FILEPATH);
    
    g_game_state.player = new Entity();
    g_game_state.player->set_position(glm::vec3(0.0f, 2.0f, 0.0f));
    g_game_state.player->set_movement(glm::vec3(0.0f));
    g_game_state.player->set_speed(1.5f);
    g_game_state.player->set_acceleration(glm::vec3(0.0f, -9.81f, 0.0f));
    g_game_state.player->m_texture_id = sprite_tileset_texture_id;
    g_game_state.player->set_entity_type(PLAYER);

    g_game_state.player->m_animation_cols    = 9;
    g_game_state.player->m_animation_rows    = 3;
    g_game_state.player->m_animation_indices = new int[2] {6,7};
    g_game_state.player->m_animation_frames  = 2;
    g_game_state.player->m_animation_index   = 0;
    g_game_state.player->m_animation_time    = 0.0f;


    g_game_state.player->set_height(0.9f);
    g_game_state.player->set_width(0.9f);

    // Jumping
    g_game_state.player->set_jumping_power(6.0f);

    // ––––– ENEMY––––– //

    g_game_state.enemies = new Entity[ENEMY_COUNT];
    //follow
    g_game_state.enemies[0].set_entity_type(ENEMY);
    g_game_state.enemies[0].set_ai_type(GUARD);
    g_game_state.enemies[0].set_ai_state(IDLE);
    g_game_state.enemies[0].m_texture_id = sprite_tileset_texture_id;
    g_game_state.enemies[0].set_position(glm::vec3(6.5f, 0.0f, 0.0f));
    g_game_state.enemies[0].set_movement(glm::vec3(0.0f));
    g_game_state.enemies[0].set_speed(0.5f);
    g_game_state.enemies[0].set_acceleration(glm::vec3(0.0f, -9.81f, 0.0f));
    //set_animation
    g_game_state.enemies[0].m_animation_cols    = 9;
    g_game_state.enemies[0].m_animation_rows    = 3;
    g_game_state.enemies[0].m_animation_indices = new int[2] {2,3};
    g_game_state.enemies[0].m_animation_frames  = 2;
    g_game_state.enemies[0].m_animation_index   = 0;
    g_game_state.enemies[0].m_animation_time    = 0.0f;

    //fly
    g_game_state.enemies[1].set_entity_type(ENEMY);
    g_game_state.enemies[1].set_ai_type(FLY);
    g_game_state.enemies[1].set_ai_state(IDLE);
    g_game_state.enemies[1].m_texture_id = sprite_tileset_texture_id;
    g_game_state.enemies[1].set_position(glm::vec3(12.0f, 2.0f, 0.0f));
    g_game_state.enemies[1].set_movement(glm::vec3(0.0f));
    g_game_state.enemies[1].set_speed(0.5f);
    g_game_state.enemies[1].set_acceleration(glm::vec3(0.0f, 0.0f, 0.0f));
    //set animation
    g_game_state.enemies[1].m_animation_cols    = 9;
    g_game_state.enemies[1].m_animation_rows    = 3;
    g_game_state.enemies[1].m_animation_indices = new int[3] {24,25,26};
    g_game_state.enemies[1].m_animation_frames  = 3;
    g_game_state.enemies[1].m_animation_index   = 0;
    g_game_state.enemies[1].m_animation_time    = 0.0f;
    
    //ram
    
    //fly
    g_game_state.enemies[2].set_entity_type(ENEMY);
    g_game_state.enemies[2].set_ai_type(RAM);
    g_game_state.enemies[2].set_ai_state(IDLE);
    g_game_state.enemies[2].m_texture_id = sprite_tileset_texture_id;
    g_game_state.enemies[2].set_position(glm::vec3(23.0f, 2.0f, 0.0f));
    g_game_state.enemies[2].set_movement(glm::vec3(0.0f));
    g_game_state.enemies[2].set_speed(0.5f);
    g_game_state.enemies[2].set_acceleration(glm::vec3(0.0f, -9.81f, 0.0f));
    //set animation
    g_game_state.enemies[2].m_animation_cols    = 9;
    g_game_state.enemies[2].m_animation_rows    = 3;
    g_game_state.enemies[2].m_animation_indices = new int[2] {21,22};
    g_game_state.enemies[2].m_animation_frames  = 2;
    g_game_state.enemies[2].m_animation_index   = 0;
    g_game_state.enemies[2].m_animation_time    = 0.0f;
    
    // ––––– AUDIO STUFF ––––– //
    Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 4096);

    g_game_state.bgm = Mix_LoadMUS(BGM_FILEPATH);
    Mix_PlayMusic(g_game_state.bgm, -1);
    Mix_VolumeMusic(MIX_MAX_VOLUME / 4.0f);

    g_game_state.jump_sfx = Mix_LoadWAV(BOUNCING_SFX_FILEPATH);

    
    FONT_TEXTURE_ID = load_texture(FONTSHEET_FILEPATH);
    // ––––– GENERAL ––––– //
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void process_input()
{
    g_game_state.player->set_movement(glm::vec3(0.0f));

    SDL_Event event;
    while (SDL_PollEvent(&event))
    {
        switch (event.type) {
            // End game
        case SDL_QUIT:
        case SDL_WINDOWEVENT_CLOSE:
            g_game_is_running = false;
            break;

        case SDL_KEYDOWN:
            switch (event.key.keysym.sym) {
            case SDLK_q:
                // Quit the game with a keystroke
                g_game_is_running = false;
                break;

            case SDLK_SPACE:
                // Jump
                if (g_game_state.state == 0 && g_game_state.player->m_collided_bottom)
                {
                    g_game_state.player->m_is_jumping = true;
                    Mix_PlayChannel(
                        NEXT_CHNL,       // using the first channel that is not currently in use...
                        g_game_state.jump_sfx,  // ...play this chunk of audio...
                        PLAY_ONCE        // ...once.
                    );
                }
                break;

            case SDLK_m:
                // Mute volume
                Mix_HaltMusic();
                break;

            default:
                break;
            }

        default:
            break;
        }
    }

    const Uint8* key_state = SDL_GetKeyboardState(NULL);

    if (g_game_state.state == 0 && key_state[SDL_SCANCODE_LEFT])
    {
        g_game_state.player->move_left();
    }
    else if (g_game_state.state == 0 && key_state[SDL_SCANCODE_RIGHT])
    {
        g_game_state.player->move_right();
    }

    // This makes sure that the player can't move faster diagonally
    if (g_game_state.state == 0 && glm::length(g_game_state.player->get_movement()) > 1.0f)
    {
        g_game_state.player->set_movement(glm::normalize(g_game_state.player->get_movement()));
    }
}

void update()
{
    float ticks = (float)SDL_GetTicks() / MILLISECONDS_IN_SECOND;
    float delta_time = ticks - g_previous_ticks;
    g_previous_ticks = ticks;

    delta_time += g_time_accumulator;

    if (delta_time < FIXED_TIMESTEP)
    {
        g_time_accumulator = delta_time;
        return;
    }
    if(g_game_state.state == 0)
        while (delta_time >= FIXED_TIMESTEP) {
            g_game_state.player->update(FIXED_TIMESTEP, g_game_state.player, g_game_state.enemies, ENEMY_COUNT, g_game_state.map);

            for (int i = 0; i < ENEMY_COUNT; i++){
                if(!g_game_state.enemies[i].get_dead()) g_game_state.enemies[i].update(FIXED_TIMESTEP, g_game_state.player, g_game_state.enemies, ENEMY_COUNT, g_game_state.map);
            }

            delta_time -= FIXED_TIMESTEP;
        }

    g_time_accumulator = delta_time;
    g_view_matrix = glm::mat4(1.0f);
    g_view_matrix = glm::translate(g_view_matrix, glm::vec3(-g_game_state.player->get_position().x, 0.0f, 0.0f));
    if(g_game_state.player->get_dead()){
        g_game_state.state = 1;
        
    }
    int count = 0;
    for(int i =0; i < ENEMY_COUNT; i++){
        count += g_game_state.enemies[i].get_dead();
    }
    if(count == ENEMY_COUNT) g_game_state.state = 2;
   
}

void render()

{
    


    g_shader_program.SetViewMatrix(g_view_matrix);
    glClear(GL_COLOR_BUFFER_BIT);

    g_game_state.player->render(&g_shader_program);
    g_game_state.map->render(&g_shader_program);

//    for (int i = 0; i < PLATFORM_COUNT; i++) g_game_state.platforms[i].render(&g_shader_program);
    for (int i = 0; i < ENEMY_COUNT; i++)    g_game_state.enemies[i].render(&g_shader_program);

    if(g_game_state.state == 1){
        lose();
    }
    else if(g_game_state.state == 2){
        win();
    }

    SDL_GL_SwapWindow(g_display_window);
}

void shutdown()
{
    SDL_Quit();

    delete   g_game_state.player;
}

// ––––– GAME LOOP ––––– //
int main(int argc, char* argv[])
{
    initialise();

    while (g_game_is_running)
    {
        
        process_input();
        update();
        render();
    }

    shutdown();
    return 0;
}
