//      __
//  ___( o)>
//  \ <_. )
//   `---'
//
// by ducktumn

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <freetype2/ft2build.h>
#include FT_FREETYPE_H

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image/stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image/stb_image_write.h"
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "stb_image/stb_image_resize2.h"

// Struct that represents a character and it's brightness value
typedef struct character {
    char character;
    double value;
} character;

// Struct that represents a folder full of frames from a video
// - All the frames are assumed to be the same dimension
// - Height is the original frame height in the folder without any modifications
typedef struct frame_folder {
    char *folder_name_and_prefix;
    int min_size_without_number;
    char *extension;
    int min_index_size;
    int start;
    int end;
    int width;
    int height;
    int original_framerate;
} frame_folder;

// Functions used in this program

int save_as_grayscale(const char *);
unsigned char *get_character_bitmap(char, const char *, int *, int *);
double get_average_brightness(unsigned char *, int);
void sort_characters(character *, int);
void scale_to_255(character *, int);
unsigned char *get_image_as_grayscale(const char *, int *, int *, int);
unsigned char *get_image_as_colored(const char *, int *, int *, int);
char *turn_to_ascii(unsigned char *, character *, int, int, int);
char *get_colored_character(char, int, int, int);
int get_size(int);
int get_closest_character_index(unsigned char, character *, int);
int get_character_set(character[]);
int print_image(char[], int, char *);
char *get_colored_double_pixel(int, int, int, int, int, int);
void get_colored_double_pixel_optimized(int, int, int, int, int, int, char *, char *, int *);
void get_colored_character_optimized(char, int, int, int, char *, char *, int *);
void play_folder(frame_folder, char *, int *, int, int);
void calculate_lookup_table(character[], char *);
void print_timeline(int, int, int, int, char *);

// Default values for the current state of the program

#define DEFAULT_FONT_PATH "assets/fonts/UbuntuMono.ttf"
#define ASCII_STARTING_POINT 32
#define ASCII_ENDING_POINT 126
#define ASCII_CHARACTER_COUNT 95
#define DEFAULT_CHARACTER_HEIGHT 22
#define DEFAULT_CHARACTER_WIDTH 10
#define FIRST_LINE_CODE "\033[H"
#define CLEAR_CODE "\033[2J"
#define RED "\033[31m"
#define RESET "\033[0m"
#define GREEN "\033[32m"
#define FULL_CLEAR "\033[2J\033[H"
#define MAXIMUM_COLORED_CHARACTER_SIZE 25
#define MAXIMUM_DOUBLE_PIXEL_SIZE 46

// The font Ubunto Mono and the size 10x22 is default for now
int main(int argc, char *argv[]) {
    // Initial Setup
    character ordered_set[ASCII_CHARACTER_COUNT] = {0};
    get_character_set(ordered_set);
    char lookup_table[256] = {0};
    calculate_lookup_table(ordered_set, lookup_table);

    // (path to the images without the number, size of the path string for any frame images excluding the numbers at the end, extension, minimum number length at the end, first frame, last frame, width, height, native fps)
    frame_folder folder = {"example_folder/frame", 24, ".png", 3, 1, 6572, 288, 216, 30};
    
    play_folder(folder, lookup_table, NULL, -1, 0);
    return 0;
}

// Assigns a character to each possible color value
// - Example: If the brightness value is 139 then the assigned character is lookup_table[139]
// - Assumes lookup_table has 256 spaces
void calculate_lookup_table(character set[], char *lookup_table) {
    for (int i = 0; i < 256; i++)
        lookup_table[i] = set[get_closest_character_index(i, set, ASCII_CHARACTER_COUNT)].character;
}

// Prints the image in the original dimensions
// - Color = -1 -> B&W
// - Color = -2 -> Colored ASCII
// - Color = -3 -> No Streching, Pixel by Pixel Display (Not ASCII)
// - Color = Any Printable Character -> Colored Single Character
int print_image(char lookup_table[], int color, char *path) {
    int width, height;
    char new_line = '\n';
    if (color == -2) {
        unsigned char *colored_image = get_image_as_colored(path, &height, &width, 1);

        char *frame_buffer = (char *)malloc(height * (width + 1) * MAXIMUM_COLORED_CHARACTER_SIZE);
        char *shared_buffer = (char *)malloc(MAXIMUM_COLORED_CHARACTER_SIZE);
        if (!frame_buffer || !shared_buffer) {
            fprintf(stderr, "Memory allocation failed in print_image()\n");
            exit(1);
        }
        int size_of_buffer = 0;
        for (int i = 0; i < height; i++) {
            for (int j = 0; j < width; j++) {
                int pixel_index = i * width * 3 + j * 3;
                int red = colored_image[pixel_index];
                int green = colored_image[pixel_index + 1];
                int blue = colored_image[pixel_index + 2];

                int gray_value = (red * 0.299) + (green * 0.587) + (blue * 0.114);
                get_colored_character_optimized(lookup_table[gray_value],
                                                red,
                                                green,
                                                blue,
                                                shared_buffer,
                                                &frame_buffer[size_of_buffer],
                                                &size_of_buffer);
            }
            frame_buffer[size_of_buffer] = '\n';
            size_of_buffer++;
        }
        write(1, frame_buffer, size_of_buffer);

        free(shared_buffer);
        free(frame_buffer);
        free(colored_image);
    } else if (color == -1) {
        unsigned char *gray_image = get_image_as_grayscale(path, &height, &width, 1);
        char *image_buffer = (char *)malloc((width + 1) * height);
        if (!image_buffer) {
            fprintf(stderr, "Memory allocation failed in print_image()\n");
            exit(1);
        }
        for (int i = 0; i < height; i++) {
            for (int j = 0; j < width; j++) {
                image_buffer[(i * (width + 1) + j)] = lookup_table[gray_image[i * width * 3 + j * 3]];
            }
            image_buffer[(i + 1) * (width + 1) - 1] = '\n';
        }
        write(1, image_buffer, (width + 1) * height);
        free(gray_image);
        free(image_buffer);
    } else if (color == -3) {
        unsigned char *colored_image = get_image_as_colored(path, &height, &width, 0);
        char *shared_buffer = (char *)malloc(MAXIMUM_DOUBLE_PIXEL_SIZE);
        char *frame_buffer = (char *)malloc((height / 2) * (width + 1) * MAXIMUM_DOUBLE_PIXEL_SIZE);
        if (!frame_buffer || !shared_buffer) {
            fprintf(stderr, "Memory allocation failed in print_image()\n");
            exit(1);
        }
        int size_of_buffer = 0;
        for (int i = 0; i < height / 2; i++) {
            int upper_index = i * 6 * width;
            int lower_index = (i * 2 + 1) * width * 3;
            for (int j = 0; j < width; j++) {
                if (lower_index >= ((height - 1) * width * 3)) {
                    get_colored_double_pixel_optimized(colored_image[upper_index + j * 3],
                                                       colored_image[upper_index + j * 3 + 1],
                                                       colored_image[upper_index + j * 3 + 2],
                                                       0,
                                                       0,
                                                       0,
                                                       shared_buffer,
                                                       &frame_buffer[size_of_buffer],
                                                       &size_of_buffer);

                }

                else {
                    get_colored_double_pixel_optimized(colored_image[upper_index + j * 3],
                                                       colored_image[upper_index + j * 3 + 1],
                                                       colored_image[upper_index + j * 3 + 2],
                                                       colored_image[lower_index + j * 3],
                                                       colored_image[lower_index + j * 3 + 1],
                                                       colored_image[lower_index + j * 3 + 2],
                                                       shared_buffer,
                                                       &frame_buffer[size_of_buffer],
                                                       &size_of_buffer);
                }
            }
            frame_buffer[size_of_buffer] = '\n';
            size_of_buffer++;
        }

        write(1, frame_buffer, size_of_buffer);
        free(frame_buffer);
        free(shared_buffer);
        free(colored_image);

    } else {
        if ((color >= ASCII_ENDING_POINT) || (color <= ASCII_STARTING_POINT)) {
            fprintf(stderr, "ASCII out of bound in print_image()\n");
            exit(1);
        }

        unsigned char *colored_image = get_image_as_colored(path, &height, &width, 1);
        char *shared_buffer = (char *)malloc(MAXIMUM_COLORED_CHARACTER_SIZE);
        char *frame_buffer = (char *)malloc(height * (width + 1) * MAXIMUM_COLORED_CHARACTER_SIZE);
        if (!frame_buffer || !shared_buffer) {
            fprintf(stderr, "Memory allocation failed in print_image()\n");
            exit(1);
        }
        int size_of_buffer = 0;
        for (int i = 0; i < height; i++) {
            for (int j = 0; j < width; j++) {
                int pixel_index = i * width * 3 + j * 3;
                get_colored_character_optimized(color,
                                                colored_image[pixel_index],
                                                colored_image[pixel_index + 1],
                                                colored_image[pixel_index + 2],
                                                shared_buffer,
                                                &frame_buffer[size_of_buffer],
                                                &size_of_buffer);
            }
            frame_buffer[size_of_buffer] = '\n';
            size_of_buffer++;
        }

        write(1, frame_buffer, size_of_buffer);
        free(shared_buffer);
        free(frame_buffer);
        free(colored_image);
    }

    fflush(stdout);
    return 0;
}

// Gets you a set of Character - Value pairs that
// represents the brightness value for each ASCII character
// and also scaled up to 255
int get_character_set(character set[]) {
    int width, height;
    for (int i = ASCII_STARTING_POINT; i < ASCII_ENDING_POINT + 1; i++) {
        unsigned char *bitmap = get_character_bitmap(i, DEFAULT_FONT_PATH, &width, &height);
        int size = width * height;
        double avg = get_average_brightness(bitmap, size);
        avg = avg * size / (DEFAULT_CHARACTER_HEIGHT * DEFAULT_CHARACTER_WIDTH);
        set[i - 32].character = i;
        set[i - 32].value = avg;
        free(bitmap);
    }

    sort_characters(set, ASCII_CHARACTER_COUNT);
    scale_to_255(set, ASCII_CHARACTER_COUNT);

    return 0;
}

// Saves the grayscale version of the image as a .png file
// - (Used for testing purposes)
int save_as_grayscale(const char *path_to_file) {
    int width, height, channels;
    unsigned char *img = stbi_load(path_to_file, &width, &height, &channels, 3);
    unsigned char *gray_img = (unsigned char *)malloc(width * height * 3);

    if (!img || !gray_img) {
        fprintf(stderr, "Memory allocation failed in save_as_grayscale()\n");
        exit(1);
    }

    for (int i = 0; i < width * height; i++) {

        unsigned char red = img[i * 3];
        unsigned char green = img[i * 3 + 1];
        unsigned char blue = img[i * 3 + 2];

        unsigned char gray = (0.299 * red) + (0.587 * green) + (0.114 * blue);

        gray_img[i * 3] = gray;
        gray_img[i * 3 + 1] = gray;
        gray_img[i * 3 + 2] = gray;
    }

    if (!stbi_write_png("output.png", width, height, 3, gray_img, width * 3)) {
        fprintf(stderr, "Failed to write PNG image to output.png in save_as_grayscale()\n");
        exit(1);
    }

    stbi_image_free(img);
    free(gray_img);
    return 0;
}

// Returns the image bitmap as grayscale
// - You can resize to half the length if you want
// - Returns a bitmap in RGB format
unsigned char *get_image_as_grayscale(const char *path_to_file, int *height, int *width, int shortened) {
    int channels;
    unsigned char *img = stbi_load(path_to_file, width, height, &channels, 3);
    if (!img) {
        fprintf(stderr, "Memory allocation failed in get_image_as_grayscale()\n");
        exit(1);
    }
    int size = *height * *width * 3;
    for (int i = 0; i < *width * *height; i++) {
        unsigned char red = img[i * 3];
        unsigned char green = img[i * 3 + 1];
        unsigned char blue = img[i * 3 + 2];

        unsigned char gray = (0.299 * red) + (0.587 * green) + (0.114 * blue);

        img[i * 3] = gray;
        img[i * 3 + 1] = gray;
        img[i * 3 + 2] = gray;
    }
    if (shortened) {
        unsigned char *resized_img = (unsigned char *)malloc(*width * (*height / 2) * 3);
        if (!resized_img) {
            fprintf(stderr, "Memory allocation failed in get_image_as_grayscale()\n");
            exit(1);
        }
        stbir_resize_uint8_srgb(
            img, *width, *height, 0,
            resized_img, *width, *height / 2, 0,
            STBIR_RGB);

        *height /= 2;
        stbi_image_free(img);
        return resized_img;
    }

    unsigned char *copy = (unsigned char *)malloc(size);
    if (!copy) {
        fprintf(stderr, "Memory allocation failed in get_image_as_grayscale()\n");
        exit(1);
    }
    memcpy(copy, img, size);
    stbi_image_free(img);
    return copy;
}

// Returns the image bitmap
// - You can resize to half the length if you want
// - Returns a bitmap in RGB format
unsigned char *get_image_as_colored(const char *path_to_file, int *height, int *width, int shortened) {
    int channels;
    unsigned char *img = stbi_load(path_to_file, width, height, &channels, 3);
    if (!img) {
        fprintf(stderr, "Memory allocation failed in get_image_as_colored()\n");
        exit(1);
    }
    int size = *height * *width * 3;

    if (shortened) {
        unsigned char *resized_img = (unsigned char *)malloc(*width * (*height / 2) * 3);
        if (!resized_img) {
            fprintf(stderr, "Memory allocation failed in get_image_as_colored()\n");
            exit(1);
        }

        stbir_resize_uint8_srgb(
            img, *width, *height, 0,
            resized_img, *width, *height / 2, 0,
            STBIR_RGB);

        stbi_image_free(img);

        *height /= 2;
        return resized_img;
    }

    unsigned char *copy = (unsigned char *)malloc(size);
    if (!copy) {
        fprintf(stderr, "Memory allocation failed in get_image_as_colored()\n");
        exit(1);
    }
    memcpy(copy, img, size);
    stbi_image_free(img);
    return copy;
}

// Returns the character glyph bitmap based on a font
unsigned char *get_character_bitmap(char character, const char *font_path, int *width_out, int *height_out) {
    FT_Library library;
    FT_Face face;

    if (FT_Init_FreeType(&library)) {
        fprintf(stderr, "Could not initialize FreeType library in get_character_bitmap()\n");
        FT_Done_Face(face);
        FT_Done_FreeType(library);
        exit(1);
    }

    if (FT_New_Face(library, font_path, 0, &face)) {
        fprintf(stderr, "Could not load font in get_character_bitmap()\n");
        FT_Done_Face(face);
        FT_Done_FreeType(library);
        exit(1);
    }

    FT_Set_Pixel_Sizes(face, 0, 22);

    if (FT_Load_Char(face, character, FT_LOAD_RENDER)) {
        fprintf(stderr, "Could not load character in get_character_bitmap()\n");
        FT_Done_Face(face);
        FT_Done_FreeType(library);
        exit(1);
    }

    FT_Bitmap bitmap = face->glyph->bitmap;

    int width = bitmap.width;
    int height = bitmap.rows;
    int pitch = bitmap.pitch;

    unsigned char *buffer = malloc(width * height);
    if (!buffer) {
        fprintf(stderr, "Memory allocation failed in get_character_bitmap()\n");
        FT_Done_Face(face);
        FT_Done_FreeType(library);
        exit(1);
    }

    for (int y = 0; y < height; y++) {
        memcpy(buffer + y * width, bitmap.buffer + y * pitch, width);
    }

    if (width_out)
        *width_out = width;
    if (height_out)
        *height_out = height;

    FT_Done_Face(face);
    FT_Done_FreeType(library);

    return buffer;
}

// Returns the average value of each pixel in a characters glyph
double get_average_brightness(unsigned char *bitmap, int size) {
    double total = 0;
    for (int i = 0; i < size; i++)
        total += bitmap[i];
    if (size == 0)
        return 0;
    else
        return (total / size);
}

// Sorts the character set based on
// their brigtness levels using Bubble Sort
void sort_characters(character *set, int size) {
    int changes_made = 1;
    while (changes_made) {
        changes_made = 0;
        for (int i = 0; i < size - 1; i++) {
            if ((set + i)->value > (set + i + 1)->value) {
                char swap_c = (set + i)->character;
                double swap_d = (set + i)->value;
                (set + i)->character = (set + i + 1)->character;
                (set + i)->value = (set + i + 1)->value;
                (set + i + 1)->value = swap_d;
                (set + i + 1)->character = swap_c;
                changes_made++;
            }
        }
    }
}

// - The original ASCII list usually has numbers between 0-150
// - Each pixel can have a value between 0-255
// - To make matching easier this function scales all the values
// - At the end the maximum value is 255
void scale_to_255(character *set, int size) {
    double multiplier = 255.0 / set[size - 1].value;
    for (int i = 0; i < size; i++) {
        set[i].value *= multiplier;
    }
}

// Turns all the values in the bitmap into ASCII art string
// - Assumes the image is grayscale/single channel (Red=Green=Blue)
char *turn_to_ascii(unsigned char *gray_img, character *set, int set_size, int width, int height) {
    char *return_ascii = (char *)malloc(width * height);
    if (!return_ascii) {
        fprintf(stderr, "Memory allocation failed in turn_to_ascii()\n");
        exit(1);
    }
    for (int i = 0; i < height; i++) {
        for (int j = 0; j < width; j++) {
            unsigned char pixel_value = gray_img[i * width * 3 + j * 3];
            return_ascii[i * width + j] = set[get_closest_character_index(pixel_value, set, set_size)].character;
        }
    }
    return return_ascii;
}

// Takes a brightness value and matches it with
// the closest character in the ASCII set using Linear Search
int get_closest_character_index(unsigned char value, character *set, int set_size) {
    int closest_index = set_size - 1;
    double latest_difference = 255.0 - value;

    for (int i = set_size - 2; i >= 0; i--) {
        double temp_difference = abs(set[i].value - value);
        if (temp_difference > latest_difference)
            break;
        else {
            closest_index = i;
            latest_difference = temp_difference;
        }
    }

    return closest_index;
}

// Turns a single character into a collection of escape codes that
// will print the character in that color if terminal supports it
char *get_colored_character(char character, int red, int green, int blue) {
    int total_size = 13 + get_size(red) + get_size(green) + get_size(blue) + 3;
    char *string = (char *)malloc(total_size);
    if (!string) {
        fprintf(stderr, "Memory allocation failed in get_colored_character()\n");
        exit(1);
    }

    // Start the ANSI escape sequence
    string[0] = '\033';
    string[1] = '[';
    string[2] = '3';
    string[3] = '8';
    string[4] = ';';
    string[5] = '2';
    string[6] = ';';

    int current_index = 7;

    // Turn RGB values to string and append it
    current_index += sprintf(&string[current_index], "%d;", red);
    current_index += sprintf(&string[current_index], "%d;", green);
    current_index += sprintf(&string[current_index], "%d", blue);

    // End the color sequence
    string[current_index] = 'm';
    current_index++;

    // Append the character to the string
    string[current_index] = character;
    current_index++;

    // Add the reset escape sequence
    string[current_index] = '\033';
    string[current_index + 1] = '[';
    string[current_index + 2] = '0';
    string[current_index + 3] = 'm';
    string[current_index + 4] = '\0';

    return string;
}

// Writes the related string to the buffer starting from that location
// - Increases the total string size accordingly
// - Assumes enough space is present
// - For the safest way of using it allocate 25*"Amount of Calls" bytes of memory
void get_colored_character_optimized(char character, int red, int green, int blue, char *temp_buffer, char *buffer_out, int *string_size) {
    sprintf(temp_buffer, "\033[38;2;%d;%d;%dm%c\033[0m", red, green, blue, character);
    int size = strlen(temp_buffer);
    memcpy(buffer_out, temp_buffer, size);
    *string_size += size;
}

// Instead of using an ASCII character this function uses
// an upper half square character from UTF-8.
// - Background is the lower pixel
// - Text color is the upper pixel
// - Returns two pixels in one character
char *get_colored_double_pixel(int red_u, int green_u, int blue_u, int red_l, int green_l, int blue_l) {
    int total_size = get_size(red_u) + get_size(green_u) + get_size(blue_u) +
                     get_size(red_l) + get_size(green_l) + get_size(blue_l) + 28;
    char *string = (char *)malloc(total_size);
    if (!string) {
        fprintf(stderr, "Memory allocation failed in get_colored_double_pixel()\n");
        exit(1);
    }

    int current_index = 0;

    // Foreground color
    current_index += sprintf(&string[current_index], "\033[38;2;%d;%d;%dm", red_u, green_u, blue_u);

    // Background color
    current_index += sprintf(&string[current_index], "\033[48;2;%d;%d;%dm", red_l, green_l, blue_l);

    // Upper half block character
    current_index += sprintf(&string[current_index], "\xE2\x96\x80");

    // Reset formatting
    current_index += sprintf(&string[current_index], "\033[0m");

    string[current_index] = '\0';

    return string;
}

// Writes the related string to the buffer starting from that location
// - Increases the total string size accordingly
// - Assumes enough space is present
// - For the safest way of using it allocate 46*"Amount of Calls" bytes of memory
void get_colored_double_pixel_optimized(int red_u, int green_u, int blue_u, int red_l, int green_l, int blue_l, char *temp_buffer, char *buffer_out, int *string_size) {
    sprintf(temp_buffer, "\033[38;2;%d;%d;%dm\033[48;2;%d;%d;%dm\xE2\x96\x80\033[0m", red_u, green_u, blue_u, red_l, green_l, blue_l);
    int size = strlen(temp_buffer);
    memcpy(buffer_out, temp_buffer, size);
    *string_size += size;
}

// Returns the size of a number excluding the leading zeros
// - ex. 0012 -> 2, 0000 -> 1
int get_size(int number) {
    int size = 0;
    while (number != 0) {
        number /= 10;
        size++;
    }
    if (size == 0) {
        return 1;
    }
    return size;
}

// Tries to play a folder full of frames in a spesific framerate
// - Mode parameter is the same as the print_image function
// - Expects a frame_folder object that is initialized correctly
// - Plays frames from folderx/frame000.png to folderx/frame1111.png in 24 fps
// - Saves a frametime.csv file for framatime analyzing if needed
// - If framerate is NULL then the original framerate from the folder will be used
// - Assumes:
// All parameters are correct
// Dimensions are consistent
void play_folder(frame_folder folder, char lookup_table[], int *framerate_target, int mode, int csv) {
    char *folder_name_and_prefix = folder.folder_name_and_prefix;
    int min_size_without_number = folder.min_size_without_number;
    char *extension = folder.extension;
    int minimum_index_size = folder.min_index_size;
    int start = folder.start;
    int end = folder.end;
    int width = folder.width;
    int height = folder.height;

    int framerate;
    if (framerate_target == NULL)
        framerate = folder.original_framerate;
    else
        framerate = *framerate_target;
    double target_ms = 1000 / framerate;

    int max_size = min_size_without_number;
    if (get_size(end) > minimum_index_size)
        max_size += get_size(end);
    else
        max_size += minimum_index_size;

    char *shared_path_buffer = (char *)malloc(max_size + 1);
    if (!shared_path_buffer) {
        fprintf(stderr, "Memory allocation failed in play_folder()\n");
        exit(1);
    }

    int frame_total = end - start + 1;
    char *timeline = (char *)malloc(width + 1);
    char *supposed_timeline = (char *)malloc(width + 1);
    if (!timeline || !supposed_timeline) {
        fprintf(stderr, "Memory allocation failed in play_folder()\n");
        exit(1);
    }
    memset(timeline, '-', width);
    timeline[width] = '\0';
    memset(supposed_timeline, '-', width);
    supposed_timeline[width] = '\0';
    double total_ms = 0;
    int supposed_frame = 0;

    FILE *file;
    if (csv) {
        file = fopen("frametime.csv", "w");
        if (!file) {
            fprintf(stderr, "Could not open frametime.csv for writing in play_folder()\n");
            exit(1);
        }
        fprintf(file, "frame,ms\n");
    }
    struct timespec start_t,
        end_t, sleep_time, remaining;

    printf(FULL_CLEAR);
    fflush(stdout);
    for (int i = start; i <= end; i++) {
        printf(FIRST_LINE_CODE);
        fflush(stdout);
        clock_gettime(CLOCK_MONOTONIC, &start_t);

        sprintf(shared_path_buffer, "%s%0*d%s", folder_name_and_prefix, minimum_index_size, i, extension);
        print_image(lookup_table, mode, shared_path_buffer);

        clock_gettime(CLOCK_MONOTONIC, &end_t);
        double elapsed_ms = (end_t.tv_sec - start_t.tv_sec) * 1000.0 +
                            (end_t.tv_nsec - start_t.tv_nsec) / 1000000.0;
        double sleep_ms = target_ms - elapsed_ms;
        if (sleep_ms > 0) {
            sleep_time.tv_sec = (time_t)(sleep_ms / 1000);
            sleep_time.tv_nsec = (long)((sleep_ms - (sleep_time.tv_sec * 1000)) * 1000000L);
            if (nanosleep(&sleep_time, &remaining) == -1) {
                if (csv)
                    fprintf(file, "%d,%lf\n", i, elapsed_ms);
                elapsed_ms += sleep_ms - (remaining.tv_sec * 1000.0) - (remaining.tv_nsec / 1000000.0);
                total_ms += elapsed_ms;
            } else {
                if (csv)
                    fprintf(file, "%d,%lf\n", i, target_ms);
                total_ms += target_ms;
            }
        } else {
            if (csv)
                fprintf(file, "%d,%lf\n", i, elapsed_ms);
            total_ms += elapsed_ms;
        }
        supposed_frame = fmin(total_ms / target_ms, frame_total);

        if ((i - start + 1) == supposed_frame)
            print_timeline(width, i - start + 1, frame_total, 0, timeline);
        else
            print_timeline(width, i - start + 1, frame_total, 1, timeline);
        printf(" -> %.3lf FPS     \n", fmin((double)framerate, 1000 / elapsed_ms));
        print_timeline(width, supposed_frame, frame_total, 0, supposed_timeline);
        printf(" -> %.3lf FPS     ", (double)framerate);
    }
    fflush(stdout);
    printf(FULL_CLEAR);
    fflush(stdout);

    free(supposed_timeline);
    free(timeline);
    free(shared_path_buffer);
    if (csv)
        fclose(file);
}

// Prints the timeline for visualization
// - Modifies a full timeline without a creating a new one
// - Color: 1 for Red, 0 for Green
void print_timeline(int width, int frame_current, int frame_total, int color, char *timeline) {
    int index = (int)round(((double)frame_current / frame_total) * (width - 1));
    timeline[index] = '#';
    if (color)
        fputs(RED, stdout);
    else
        fputs(GREEN, stdout);
    fputs(timeline, stdout);
    fputs(RESET, stdout);
    timeline[index] = '-';
}
