#pragma once

#include <memory>
#include <cstdio>
#include <SDL2/SDL.h>
#include <tree_sitter/api.h>

template <auto Fn>
struct Deleter {
    template <typename T>
    void operator()(T* ptr) const {
        if (ptr) Fn(ptr);
    }
};

template <typename T, auto Fn>
using Handle = std::unique_ptr<T, Deleter<Fn>>;

using WindowPtr = Handle<SDL_Window, SDL_DestroyWindow>;
using RendererPtr = Handle<SDL_Renderer, SDL_DestroyRenderer>;
using SurfacePtr = Handle<SDL_Surface, SDL_FreeSurface>;
using TexturePtr = Handle<SDL_Texture, SDL_DestroyTexture>;
using CursorPtr = Handle<SDL_Cursor, SDL_FreeCursor>;

using TSParserPtr = Handle<TSParser, ts_parser_delete>;
using TSTreePtr = Handle<TSTree, ts_tree_delete>;
using TSQueryPtr = Handle<TSQuery, ts_query_delete>;
using TSQueryCursorPtr = Handle<TSQueryCursor, ts_query_cursor_delete>;

struct PipeDeleter {
    void operator()(FILE* f) const { if (f) pclose(f); }
};
using PipeHandle = std::unique_ptr<FILE, PipeDeleter>;
