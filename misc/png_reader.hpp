#include <png.h>
#include <array>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <vector>
#include <stdexcept>

namespace png {
    struct Image {
        std::size_t rb;
        std::uint32_t height, width;    
        std::vector<std::uint8_t> data;

        [[nodiscard]] std::array<std::uint8_t, 4> operator()(std::size_t row, std::size_t col) const {
            if (col >= width || row >= height) 
                throw std::out_of_range("Pixel access out of range");

            std::size_t idx {(row * rb) + (col * 4)};
            return { data[idx + 0], data[idx + 1], data[idx + 2], data[idx + 3] };
        }
    };

    // Callback needed since we are passing in ifstream when it expects FILE*
    // Get pointer we set at the `set_read_fn`, read from stream into out_bytes
    inline void png_read_callback(png_structp png_ptr, png_bytep out_bytes, png_size_t byte_count_to_read) {
        std::istream* stream {static_cast<std::istream*>(png_get_io_ptr(png_ptr))};
        stream->read(reinterpret_cast<char*>(out_bytes), static_cast<std::streamsize>(byte_count_to_read));
        if (!(*stream)) throw std::runtime_error("Read error");
    }

    inline Image read(const std::string &filename) {
        std::ifstream ifs {filename, std::ios::binary};
        if (!ifs) throw std::runtime_error("File open failed");

        // Read PNG header
        png_byte header[8];
        ifs.read(reinterpret_cast<char*>(&header), 8);
        if (png_sig_cmp(header, 0, 8)) throw std::runtime_error("Not a PNG file");

        // Init read struct
        png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
        if (!png_ptr) throw std::runtime_error("png_create_read_struct failed");
        png_infop info_ptr {png_create_info_struct(png_ptr)};
        if (!info_ptr) throw std::runtime_error("png_create_info_struct failed");

        // setjmp / longjmp - png's built in error handling. In case of internal errors
        // thrown it `jumps` here and we can throw a C++ style exception
        if (setjmp(png_jmpbuf(png_ptr))) throw std::runtime_error("PNG read error");

        // Use custom C++ stream read callback, tell png that we already read header
        png_set_read_fn(png_ptr, static_cast<void*>(&ifs), png_read_callback);
        png_set_sig_bytes(png_ptr, 8);

        // Read Meta
        png_read_info(png_ptr, info_ptr);
        std::uint32_t width {png_get_image_width(png_ptr, info_ptr)};
        std::uint32_t height {png_get_image_height(png_ptr, info_ptr)};
        png_byte color_type {png_get_color_type(png_ptr, info_ptr)};
        png_byte bit_depth  {png_get_bit_depth(png_ptr, info_ptr)};

        // Normalize image format
        // - Force 16-bit to 8-bit
        // - Pallete to RGB
        // - Grayscale to 8-bit
        // - Transparency to Alpha
        // - If no alpha, add `opaque`
        // - Grayscale to `RGB`
        // - Set the updated info
        if (bit_depth == 16) png_set_strip_16(png_ptr);
        if (color_type == PNG_COLOR_TYPE_PALETTE) png_set_palette_to_rgb(png_ptr);
        if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8) png_set_expand_gray_1_2_4_to_8(png_ptr);
        if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS)) png_set_tRNS_to_alpha(png_ptr);
        if (color_type == PNG_COLOR_TYPE_RGB || color_type == PNG_COLOR_TYPE_GRAY) png_set_add_alpha(png_ptr, 0xff, PNG_FILLER_AFTER);
        if (color_type == PNG_COLOR_TYPE_GRAY || color_type == PNG_COLOR_TYPE_GRAY_ALPHA) png_set_gray_to_rgb(png_ptr);
        png_read_update_info(png_ptr, info_ptr);

        // Sanity check to ensure RGBA
        if (png_get_channels(png_ptr, info_ptr) != 4)
            throw std::runtime_error("Expected a 4 channel RGBA output.");

        // More meta - no of bytes in single row; PNG sometimes pads op for alignment
        std::size_t rowBytes {png_get_rowbytes(png_ptr, info_ptr)};

        // Init vector for reading image
        std::vector<std::uint8_t> image_data(height * rowBytes);
        std::vector<png_bytep> row_pointers(height);
        for (std::size_t  i = 0; i < height; ++i)
            row_pointers[i] = &image_data[i * rowBytes];

        // Read image and cleanup
        png_read_image(png_ptr, row_pointers.data());
        png_destroy_read_struct(&png_ptr, &info_ptr, nullptr);

        return {.rb=rowBytes, .height=height, .width=width, .data=std::move(image_data)};
    }
}
