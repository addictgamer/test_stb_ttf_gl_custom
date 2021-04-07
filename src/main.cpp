
#include <iostream>
using std::cout;
using std::cerr;
#include <fstream>
using std::ifstream;
#include <string>
using std::string;
#include <cstdio>
#include <vector>
using std::vector;
#include <unordered_map>
using std::unordered_map;

#define GL_GLEXT_PROTOTYPES

#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>

#include "stb/stb_truetype.h"

static const int SCREEN_WIDTH = 608;
static const int SCREEN_HEIGHT = 400;

size_t getFileSize(string filepath)
{
	ifstream file(filepath, std::ios::binary);
	if (!file)
	{
		printf("ERROR: Failed to load \"%s\"\n", filepath.c_str());
		return 0;
	}

	file.seekg(0, std::ios::end);
	return file.tellg();
}

struct FontFile
{
	size_t buffer_size;
	uint8_t* buffer;

	FontFile(size_t buffer_size, uint8_t* buffer)
		: buffer_size(buffer_size),
		buffer(buffer)
	{
	}

	~FontFile()
	{
		delete[] buffer;
	}
};

FontFile* loadFontFile(string filepath)
{
	size_t filesize = getFileSize(filepath);
	if (filesize == 0)
	{
		printf("ERROR: Skipping loading empty font file \"%s\"!\n");
		return nullptr;
	}

	ifstream file(filepath, std::ios::binary);
	if (!file)
	{
		printf("ERROR: Failed to load font file \"%s\"\n", filepath.c_str());
		return nullptr;
	}

	uint8_t* buffer = new uint8_t[filesize];
	if (!file.read(reinterpret_cast<char*>(buffer), filesize))
	{
		printf("ERROR: Failed to read file \"%s\" into buffer!\n");
		return nullptr;
	}

	return new FontFile(filesize, buffer);
}

typedef unsigned char* STBTTFBitmap;

struct STBTTF_Font_Wrapper
{
	stbtt_fontinfo font;
	int pixel_height;
	float scale;
	FontFile* font_file;
	int ascent;
	int baseline;

	STBTTF_Font_Wrapper(stbtt_fontinfo& font, int pixel_height, float scale, FontFile* font_file, int ascent, int baseline) //TODO: Some of these can just be calculated within this constructor instead of in the caller...
		: 	font(font),
			pixel_height(pixel_height),
			scale(scale),
			font_file(font_file),
			ascent(ascent),
			baseline(baseline)
	{ }

	~STBTTF_Font_Wrapper()
	{
		if (nullptr != font_file)
		{
			delete font_file;
		}
	}
};

struct STBTTF_Bitmap_Wrapper
{
	string original_character;
	STBTTFBitmap bitmap;
	int width, height, x_offset, y_offset;

	STBTTF_Bitmap_Wrapper(string original_character, STBTTFBitmap bitmap, int width, int height, int x_offset, int y_offset)
		:	original_character(original_character),
			bitmap(bitmap),
			width(width),
			height(height),
			x_offset(x_offset),
			y_offset(y_offset)
	{ }

	~STBTTF_Bitmap_Wrapper()
	{
		cout << "Deconstructing a STBTTF_Bitmap_Wrapper!"; //Debug output because I'm worried about a double free on a bitmap...
		if (nullptr != bitmap)
		{
			stbtt_FreeBitmap(bitmap, nullptr); //TODO: What is the second parameter, "userdata"?
		}
	}
};

struct STBTTF_Character_Bitmap_Array_Wrapper
{
	vector<STBTTF_Bitmap_Wrapper*> bitmaps;

	~STBTTF_Character_Bitmap_Array_Wrapper()
	{
		for (auto bitmap : bitmaps)
		{
			delete bitmap;
		}
	}
};

STBTTF_Character_Bitmap_Array_Wrapper* renderTextToBitmap(STBTTF_Font_Wrapper& font, string text)
{
	STBTTF_Character_Bitmap_Array_Wrapper* characters = new STBTTF_Character_Bitmap_Array_Wrapper();

	for (unsigned i = 0; i < text.length(); ++i)
	{
		int character_width, character_height, x_offset, y_offset;
		STBTTFBitmap raw_bitmap = stbtt_GetCodepointBitmap(&font.font, font.scale, font.scale, text[i], &character_width, &character_height, &x_offset, &y_offset);
		STBTTF_Bitmap_Wrapper* wrapped_bitmap = new STBTTF_Bitmap_Wrapper(string(1, text[i]), raw_bitmap, character_width, character_height, x_offset, y_offset);
		characters->bitmaps.push_back(wrapped_bitmap);
	}

	return characters; //TODO: Take the output of this and process it into actual pixel data...
}

//TODO: For rendering, take every char bitmap and render it as its own texture that gets stored in some lookup table, indexing character value to GL texture id.

unordered_map<char, GLuint> character_texture_cache; //TODO: Key is basically a character. Char is a uint8_t which won't work for unicode, so we need to use a string (I feel is slow) or maybe uint32_t and cast to that? //TODO: In reality, this would be keyed on character and size, styling options, etc...expensive cache in practice! (Although for certain use cases may still be much better than caching every string you'll ever see)
unsigned long num_misses = 0;

//Processes every character in the bitmap array, converts its bitmap into an OpenGL texture, and stores it in a lookup table/dictionary.
void cacheRenderedSTBTTFGlyphs(const STBTTF_Character_Bitmap_Array_Wrapper& characters)
{
	for (auto character_bitmap : characters.bitmaps)
	{
		if (character_texture_cache.find(character_bitmap->original_character[0]) == character_texture_cache.end())
		{
			//TODO: Generate the texture and cache.
			num_misses++;

			GLuint texture_id;
			glGenTextures(1, &texture_id);
			glBindTexture(GL_TEXTURE_2D, texture_id);

			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, character_bitmap->width, character_bitmap->height, 0, GL_ALPHA, GL_UNSIGNED_BYTE, character_bitmap->bitmap);
			//TODO: Can free the bitmap.
			character_texture_cache[character_bitmap->original_character[0]] = texture_id;

			printf("[Cache Miss %d -> %d] Did not find character '%c' in texture cache, generating texture.\n", num_misses, texture_id, character_bitmap->original_character[0]);
		}
		else
		{
			//TODO: Might be better to make it return the found entry, so that you could use this function to cache new characters as you request them.
		}
		
	}
}

bool displayed = false;

//TODO: Destination coordinates and stuff.
void renderTextToScreen(const STBTTF_Character_Bitmap_Array_Wrapper& characters)
{
	cacheRenderedSTBTTFGlyphs(characters); //TODO: Call this on a per-character basis, if needed, while looping below!

	glDisable(GL_DEPTH_TEST);
	glDisable(GL_LIGHTING);
	glMatrixMode(GL_PROJECTION);
	glViewport(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
	glLoadIdentity();
	glOrtho(0, SCREEN_WIDTH, SCREEN_HEIGHT, 0, -1, 1);
	//glMatrixMode(GL_MODELVIEW);

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glEnable(GL_TEXTURE_2D);
	int x = 100;
	int y = 100;
	for (auto character : characters.bitmaps)
	{
		GLuint texture_id = character_texture_cache[character->original_character[0]];
		int width = character->width;
		int height = character->height;
		glBindTexture(GL_TEXTURE_2D, texture_id);
		glBegin(GL_QUADS);

		glTexCoord2f(0, 0);
		glVertex2f(x, y);
		glTexCoord2f(0, 1);
		glVertex2f(x, y + height);
		glTexCoord2f(1, 1);
		glVertex2f(x + width, y + height);
		glTexCoord2f(1, 0);
		glVertex2f(x + width, y);

		//TODO: This doesn't at all take into account things like baseline...should really use the baseline and ascent formulas.

		glEnd();

		x += width + 4;
		//TODO: Handle newlines with y?
		//if (original_character == '\n') { y += whatever; }

		//Debuggerings infos.
		if (!displayed)
		{
			printf("(%d, %d) rendered a '%c' with texture %d\n", x, y, character->original_character[0], texture_id);
		}
	}

	glDisable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, 0);

	displayed = true;
}

int main()
{
	if (SDL_Init(SDL_INIT_VIDEO) != 0)
	{
		std::cerr << "SDL_Init() Error: " << SDL_GetError() << std::endl;

		return 1;
	}

	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 1);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);

	SDL_Window *window = SDL_CreateWindow("STB TTF TEST", 100, 100, SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_SHOWN | SDL_WINDOW_OPENGL);
	if (window == nullptr)
	{
		std::cerr << "SDL_CreateWindow() Error: " << SDL_GetError() << std::endl;

		SDL_DestroyWindow(window);
		SDL_Quit();

		return 1;
	}

	SDL_GLContext context = SDL_GL_CreateContext(window);
	if (context == nullptr)
	{
		std::cerr << "SDL_GL_CreateContext() Error: " << SDL_GetError() << std::endl;

		SDL_DestroyWindow(window);
		SDL_Quit();

		SDL_DestroyWindow(window);
		SDL_Quit();

		return 1;
	}

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();

	GLenum error = GL_NO_ERROR;
	if ((error = glGetError()) != GL_NO_ERROR)
	{
		std::cerr << "Error initializing OpenGL! " << std::endl << std::endl;
		SDL_DestroyWindow(window);
		SDL_Quit();

		return 1;
	}

	// Initialize Modelview matrix
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	if ((error = glGetError()) != GL_NO_ERROR)
	{
		std::cerr << "Error initializing OpenGL! " << std::endl << std::endl;
		SDL_DestroyWindow(window);
		SDL_Quit();

		return 1;
	}

	// Set clear color
	glClearColor(0.f, 0.f, 0.f, 0.f);
	
	if ((error = glGetError()) != GL_NO_ERROR)
	{
		std::cout << "Error initializing OpenGL! " << std::endl << std::endl;
		SDL_DestroyWindow(window);
		SDL_Quit();

		return 1;
	}

	glShadeModel(GL_SMOOTH);
	glClearDepth(1.0f);
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);
	glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
	
	if ((error = glGetError()) != GL_NO_ERROR)
	{
		std::cout << "Error initializing OpenGL! " << std::endl << std::endl;
		SDL_DestroyWindow(window);
		SDL_Quit();

		return 1;
	}

	SDL_Event event;
	bool quit = false;

	int width, height;
	SDL_Surface* image = SDL_LoadBMP("data/test.bmp");
	if (image == nullptr)
	{
		cout << "Boo! Failed to load image data/test.bmp!\n";
		SDL_DestroyWindow(window);
		SDL_Quit();

		return -1;
	}

	GLenum data_format = GL_BGR;
	GLuint texture_id;
	glGenTextures(1, &texture_id);
	glBindTexture(GL_TEXTURE_2D, texture_id);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, image->w, image->h, 0, data_format, GL_UNSIGNED_BYTE, image->pixels);
	SDL_FreeSurface(image);

	stbtt_fontinfo font;

	string font_path = "data/SatellaRegular-ZVVaz.ttf";
	size_t font_filesize = getFileSize(font_path);
	cout << "Font filesize: " << font_filesize << "\n";

	FontFile *font_file = loadFontFile(font_path);
	if (!font_file)
	{
		cerr << "Failed to load font file, aborting!\n";
		SDL_DestroyWindow(window);
		SDL_Quit();

		return -1;
	}
	stbtt_InitFont(&font, font_file->buffer, 0);
	int pixel_size = 15;
	float scale = stbtt_ScaleForPixelHeight(&font, pixel_size);

	int ascent;
	stbtt_GetFontVMetrics(&font, &ascent, 0, 0);
	int baseline = static_cast<int>(ascent * scale);

	STBTTF_Font_Wrapper font_wrapper(font, pixel_size, scale, font_file, ascent, baseline);
	STBTTF_Character_Bitmap_Array_Wrapper* word_bitmaps = renderTextToBitmap(font_wrapper, "This. Is. Even. More. For. Barony!!!");
	cacheRenderedSTBTTFGlyphs(*word_bitmaps);

	while (!quit)
	{
		while (SDL_PollEvent(&event))
		{
			if (event.type == SDL_QUIT)
			{
				quit = true;
			}
		}

		glClear(GL_COLOR_BUFFER_BIT);

		glDisable(GL_DEPTH_TEST);
		glDisable(GL_LIGHTING);
		glMatrixMode(GL_PROJECTION);
		glViewport(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
		glLoadIdentity();
		glOrtho(0, SCREEN_WIDTH, 0, SCREEN_HEIGHT, -1, 1);
		glMatrixMode(GL_MODELVIEW);

		glBindTexture(GL_TEXTURE_2D, texture_id);
		glEnable(GL_TEXTURE_2D);

		glBegin(GL_QUADS);

		glTexCoord2f(0, 0);
		glVertex2f(50, SCREEN_HEIGHT - 50);
		glTexCoord2f(0, 1);
		glVertex2f(50, 50);
		glTexCoord2f(1, 1);
		glVertex2f(SCREEN_WIDTH - 50, 50);
		glTexCoord2f(1, 0);
		glVertex2f(SCREEN_WIDTH - 50, SCREEN_HEIGHT - 50);

		glEnd();

		glDisable(GL_TEXTURE_2D);
		glBindTexture(GL_TEXTURE_2D, 0);

		renderTextToScreen(*word_bitmaps);

		SDL_GL_SwapWindow(window);
	}

	//delete font_file;
	glDeleteTextures(1, &texture_id);

	SDL_DestroyWindow(window);
	SDL_Quit();

	return 0;
}
