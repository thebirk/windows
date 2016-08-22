#include <stdio.h>
#include <SDL2/SDL.h>
#include <stdbool.h>
#include <stdint.h>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;

#include "data.c"

typedef struct {
	int x;
	int y;
	int width;
	int height;
} WindowPos;

typedef struct {
    u8 r;
    u8 g;
    u8 b;
    u8 a;
} Color;

typedef enum {
    EVENT_MOUSEDOWN = 0,
    EVENT_MOUSEUP,
    EVENT_KEYDOWN,
    EVENT_KEYUP,
    EVENT_MOUSEMOVE,
} EventType;

typedef struct {
    EventType type;
    int state; // 1 down or 0 up
    int keycode;
} KeyEvent;

typedef struct {
    EventType type;
    int state; // 1 down or 0 up
    int button;
    int x;
    int y;
} MouseEvent;

typedef struct {
    EventType type;
    
    union {
        KeyEvent key;
        MouseEvent mouse;
    };
} Event;

#define TITLE_MAX_LENGTH 512
typedef struct {
    bool in_use;
    WindowPos pos;
    bool visible;
    bool decorated;
    char title[TITLE_MAX_LENGTH];
} Window;

#define WINDOWS_MAX 4096
typedef struct {
    Window windows[WINDOWS_MAX];
    Color background_color;
    bool alive;
    int mouse_x;
    int mouse_y;
    int decoration_width;
    int decoration_height;
    Color decoration_color;
} Desktop;

int NextWindowId(Desktop *d)
{
    int id = 0;
    int i = 0;
    while(d->windows[i].in_use) {
        id++;
        i++;
        if(i >= WINDOWS_MAX) return -1;
    }
    
    return id;
}

int CreateWindow(Desktop *d, WindowPos pos)
{
    int window_id = NextWindowId(d);
    if(window_id < 0) return window_id;
    
    Window *w = &d->windows[window_id];
    w->pos = pos;
    w->in_use = true;
    w->visible = false;
    w->decorated = true;

	return window_id;
}

Window* GetWindowById(Desktop *d, int window)
{
    Window *w = 0;
    
    if(window < 0 || window >= WINDOWS_MAX) {
        return 0;
    }
    if(d->windows[window].in_use) {
        w = &d->windows[window];
    }
    
    return w;
}

void WindowSetVisible(Desktop *d, int windowid, bool visible)
{
    Window *w = GetWindowById(d, windowid);
    w->visible = visible;
}

typedef struct {
    int width;
    int height;
    SDL_Window *window;
    SDL_Renderer *renderer;
    
    SDL_Texture *cursor;
    int cursor_width;
    int cursor_height;
} RenderData;

void InitRenderData(RenderData *rd, int width, int height)
{
    SDL_Init(SDL_INIT_EVERYTHING);
    
    rd->width = width;
    rd->height = height;
    SDL_CreateWindowAndRenderer(width, height, SDL_WINDOW_RESIZABLE, &rd->window, &rd->renderer);

    int w, h, n;
    u8 *data = stbi_load("cursor.png", &w, &h, &n, 4);
    rd->cursor_width = w;
    rd->cursor_height = h;
    
    SDL_Surface *cursor_surface = SDL_CreateRGBSurfaceFrom(
        data,
        w, h,
        32, 4*w,
        0x00FF0000,
        0x0000FF00,
        0x000000FF,
        0xFF000000
        );
    
    rd->cursor = SDL_CreateTextureFromSurface(
        rd->renderer,
        cursor_surface
        );
    SDL_FreeSurface(cursor_surface);
    stbi_image_free(data);
    
    SDL_ShowCursor(0);
    SDL_SetWindowTitle(rd->window, "windows: Mouse grab: no");
}

void RenderWindowDecoration(Desktop *d, RenderData *rd,  Window *w)
{
    if(!w->decorated) {
        return;
    }
    
    // Title bar
    SDL_Rect dst = {0};
    dst.x = w->pos.x - d->decoration_width;
    dst.y = w->pos.y - d->decoration_height;
    dst.w = w->pos.width + (d->decoration_width * 2);
    dst.h = d->decoration_height;
    SDL_SetRenderDrawColor(
        rd->renderer,
        d->decoration_color.r,
        d->decoration_color.g,
        d->decoration_color.b,
        d->decoration_color.a
        );
    SDL_RenderFillRect(rd->renderer, &dst);
    
    // Left bar
    dst.x = w->pos.x - d->decoration_width;
    dst.y = w->pos.y;
    dst.w = d->decoration_width;
    dst.h = w->pos.height;
    SDL_RenderFillRect(rd->renderer, &dst);
    
    // Right bar
    dst.x = w->pos.x + w->pos.width;
    dst.y = w->pos.y;
    dst.w = d->decoration_width;
    dst.h = w->pos.height;
    SDL_RenderFillRect(rd->renderer, &dst);
    
    // Bottom bar
    dst.x = w->pos.x - d->decoration_width;
    dst.y = w->pos.y + w->pos.height;
    dst.w = w->pos.width + (d->decoration_width * 2);
    dst.h = d->decoration_width;
    SDL_RenderFillRect(rd->renderer, &dst);
}

void RenderDesktop(Desktop *d, RenderData *rd)
{
    SDL_SetRenderDrawColor(
        rd->renderer,
        d->background_color.r,
        d->background_color.g,
        d->background_color.b,
        d->background_color.a
        );
    SDL_RenderClear(rd->renderer);
    
    // TODO: Render windows
    int i;
    for(i = 0; i < WINDOWS_MAX; i++) {
        if(d->windows[i].in_use && d->windows[i].visible) {
            Window *w = &d->windows[i];
            SDL_SetRenderDrawColor(rd->renderer, 0, 0, 0, 255);
            SDL_Rect r = {w->pos.x,w->pos.y,w->pos.width,w->pos.height};
            SDL_RenderFillRect(rd->renderer, &r);
            RenderWindowDecoration(d, rd, w);
        }
    }
    
    SDL_Rect dst = {0};
    dst.x = d->mouse_x;
    dst.y = d->mouse_y;
    dst.w = rd->cursor_width;
    dst.h = rd->cursor_height;
    SDL_RenderCopy(rd->renderer, rd->cursor, NULL, &dst);
    
    SDL_RenderPresent(rd->renderer);
}

void ToggleMouseGrab(RenderData *rd)
{
    bool grab = !SDL_GetWindowGrab(rd->window);
    SDL_SetWindowGrab(rd->window, grab);
    
    char buffer[1024];
    sprintf(buffer, "windows: Mouse grab: %s", grab ? "yes" : "no");
    SDL_SetWindowTitle(
        rd->window, 
        buffer
        );
}

void PollEvents(Desktop *d, RenderData *rd)
{
    SDL_Event e;
    
    while(SDL_PollEvent(&e)) {
        switch(e.type) {
            case SDL_QUIT: {
                d->alive = false;
            } break;
            
            case SDL_KEYDOWN: {
                if(e.key.keysym.sym == SDLK_F10 &&
                   e.key.keysym.mod & KMOD_SHIFT) {
                    ToggleMouseGrab(rd);
                }
            } break;
            
            case SDL_MOUSEBUTTONDOWN: {
                
            } break;
            
            case SDL_MOUSEBUTTONUP: {
                
            } break;
            
            case SDL_MOUSEMOTION: {
                d->mouse_x = e.motion.x;
                d->mouse_y = e.motion.y;
            } break;
            // Handle other events and generate and send them to windows
        }
    }
}

typedef struct {
    int sfd;
    int port;
} Server;

void InitServer(Server *s, int port, int que)
{
    s->port = port;
    s->sfd = socket(AF_INET, SOCK_STREAM|SOCK_NONBLOCK, 0);
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);
    bind(s->sfd, &addr, sizeof(addr));
    listen(s->sfd, que);
}

int StringLength(char *str)
{
    int len = 0;
    while(*str) {
        len++;
        str++;
    }
    return len;
}

bool StringCompare(char *s1, char *s2)
{
    if(s1 == 0 || s2 == 0) return false;
    int s1_len = StringLength(s1);
    int s2_len = StringLength(s2);
    
    if(s1_len != s2_len) return false;
    
    int matches = 0;
    while(*s1 && *s2 && (*s1 == *s2)) {
        matches++;
        s1++;
        s2++;
    }
    
    return matches == s1_len;
}

bool StringStartsWith(char *str, char *part)
{
    if(str == 0 || part == 0) return false;
    int str_len = StringLength(str);
    int part_len = StringLength(part);
    
    if(str_len < part_len) return false;
    
    int matches = 0;
    while(*str && *part && (*str == *part)) {
        matches++;
        str++;
        part++;
        if(matches == part_len) return true;
    }
    
    return false;
}

void WriteStringToBuffer(char *buffer, char *str)
{
    while(*str) {
        *buffer = *str;
        buffer++;
        str++;
    }
}

#define BUFFER_SIZE 2048
void* ServeClient(void *arg)
{
    int cfd = (int) arg;
    printf("Serving client! sfd: %d\n", cfd);
    
    char buffer[BUFFER_SIZE] = "HELLO\r\n";
    write(cfd, buffer, StringLength(buffer));
    
    while(true) {
        memset(buffer, 0, BUFFER_SIZE);
        int ret = read(cfd, buffer, BUFFER_SIZE);
        if(ret == -1) {
            continue;
        }
        buffer[BUFFER_SIZE-1] = 0;
        
        if(StringStartsWith(buffer, "BYE")) {
            printf("Client sent BYE!\n");
            WriteStringToBuffer(buffer, "GOODBYE\r\n");
            write(cfd, buffer, StringLength(buffer));
            break;
        } else if(StringStartsWith(buffer, "CREATE")) {
            
        }
    
        printf("%s", buffer);
    }
    
    close(cfd);
    
    return 0;
}

void AcceptClient(Server *s)
{
    int cfd = accept(s->sfd, NULL, NULL);
    if(cfd > 0) {
        pthread_t thread_id = 0;
        pthread_create(&thread_id, NULL, ServeClient, (void*)cfd);
    }
}

void InitDesktop(Desktop *d)
{
    d->alive = true;
    d->background_color.r = 0x7F;
    d->background_color.g = 0x7F;
    d->background_color.b = 0x7F;
    d->background_color.a = 0xFF;
    d->decoration_height = 18;
    d->decoration_width = 4;
    d->decoration_color.r = 0xFF,
    d->decoration_color.g = 0x00;
    d->decoration_color.b = 0x00;
    d->decoration_color.a = 0xFF;
}

int main(int argc, char **argv)
{
    Desktop d = {0};
    InitDesktop(&d);
    
    RenderData rd = {0};
    InitRenderData(&rd, 640, 480);
    
    Server s = {0};
    InitServer(&s, 22341, 32);
    
    WindowPos pos = {50, 50, 200, 150};
    int wid = CreateWindow(&d, pos);
    WindowSetVisible(&d, wid, true);
    
    while(d.alive) {
        PollEvents(&d, &rd);
        AcceptClient(&s);
        
        
        
        RenderDesktop(&d, &rd);
    }
}
