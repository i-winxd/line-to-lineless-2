#include <memory>
#include <iostream>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include <queue>
#include <unordered_set>
#define STB_IMAGE_WRITE_IMPLEMENTATION
static const int MIN_OPACITY = 50;

// static const int MIN_OPACITY_ALIAS = 100;

#include "stb_image_write.h"


std::vector<unsigned char> forceDimensionsTo4(std::vector<unsigned char> prev){
    if(prev.size() == 4){
        return prev;
    }
    if(prev.size() == 3){
        std::vector<unsigned char> newVector(prev);
        newVector.push_back(255);
        return newVector;
    }
    if(prev.size() == 1){
        return {prev[0], prev[0], prev[0], 255};
    }
    throw std::runtime_error("You couldn't have had an image that isn't 1, 3, or 4 channels");
}



struct Image {
    std::vector<unsigned char> data;
    int width;
    int height;
    int channels;

    /**
     * Create a completely blank image (all channels set to 0).
     * @param width how wide it is, >=1
     * @param height how tall it is, >=1
     * @param channels how many channels (valid: 1, 3, 4)
     */
    Image(int width, int height, int channels) : width(width), height(height), channels(channels), data(width * height * channels, 0) {
        assert(width >= 1);
        assert(height >= 1);
        assert(channels == 1 || channels == 3 || channels == 4);
    }

    /**
     * Creates an image based on a filename.
     * @param filename The filename in question
     */
    explicit Image(const std::string& filename) : width(0), height(0), channels(0) {
        unsigned char* raw_data = stbi_load(filename.c_str(), &width, &height, &channels, 0);
        if (raw_data == nullptr) {
            throw std::runtime_error("Error loading image");
        }
        data = std::vector<unsigned char>(raw_data, raw_data + width * height * channels);
        stbi_image_free(raw_data);
        if (width < 1) {
            throw std::runtime_error("Width must be at least 1");
        }

        if (height < 1) {
            throw std::runtime_error("Height must be at least 1");
        }
        assert(data.size() == width * height * channels);
    }

    [[nodiscard]] int getOffset(int x, int y) const {
        return x + y * width;
    }

    void saveImage(const std::string &filename) const {
        stbi_write_png(filename.c_str(), width, height, channels, data.data(), width*channels);
    }

    /**
     * Given offset, recalculate where the offset would be after some
     * translation
     * @param offset the initial offset
     * @param opt the new offset
     * @return whether opt was successfully calculated; will fail if opt is oob
     */
    [[nodiscard]] bool movePosition(int offset, int & opt, int dx, int dy) const {
        int x;
        int y;
        getPosition(offset, x, y);
        x = x + dx;
        y = y + dy;
        if (x < 0 || x >= width || y < 0 || y >= height){
            return false;
        } else {
            opt = getOffset(x, y);
            return true;
        }
    }

    void getPosition(int offset, int & x, int & y) const {
        x = offset % width;
        y = offset / width;
    }

    /**
     * Returns the pixel in question.
     * The length of the vector will always be == channels
     * @param x
     * @param y
     * @return
     */
    [[nodiscard]] std::vector<unsigned char> getPixel(int index) const {
        assert(index < width * height);
        return {data.begin() + index * channels, data.begin() + index * channels + channels};
    }

    void insertPixel(std::vector<unsigned char> pixel, int offset) {
        if(pixel.size() != channels){
            throw std::runtime_error("Insert pixel size mismatch");
        }
        for(int i = 0; i < channels; i++){
            data[offset * channels + i] = pixel[i];
        }
    }
};



bool checker(const std::vector<unsigned char> & pixel){
    if(pixel.size() != 4){
        return true;
    }
    return ((int) pixel[3]) > MIN_OPACITY;
}


/**
 *
 * @param img the image that we are looking at
 * @param id_offset the coordinate of the image to check
 * @param id_output gets set to the output position
 * @param limit max loops
 * @return true if I could find something, false if I couldn't
 */
bool getNearestPixel(const Image & img, int id_offset,
                     int & id_output, int limit) {
    // visited will always store
    // the pixel offset
    std::unordered_set<int> visited;
    std::queue<int> q;
    q.push(id_offset);
    int i = 0;
    while(!q.empty() && i < limit){
        int offset = q.front();
        q.pop();
        if (!visited.contains(offset)){
            visited.insert(offset);
            if(checker(img.getPixel(offset))){
                id_output = offset;
                return true;
            }
            for(int j = 0; j < 2; j++){
                for(int k = 0; k < 2; k++){
                    int p;
                    if(img.movePosition(offset, p, j * 2 - 1, k * 2 - 1)){
                        q.push(p);
                    }
                }
            }
        }
        i++;
    }
    return false;



}

bool alphaGreaterThan(const std::vector<unsigned char> & pixel,
                      int alpha){
    switch (pixel.size()){
        case 4: return pixel[3] > alpha;
        default: return true;
    }
}


//void alias(std::vector<unsigned char> & pixel){
//    assert(pixel.size() == 4);
//    pixel[3] = pixel[3] < MIN_OPACITY_ALIAS ? 0 : 255;
//}

bool replaceStrokePixels(const Image & stroke, const Image & color,
                         Image & img){

    if(stroke.width != color.width || stroke.height != color.height || stroke.channels != color.channels || color.channels != 4){
        return false;
    }
    int noPixels = stroke.width * stroke.height;
    assert(noPixels * color.channels == color.data.size());
    // new image to put the data in
    img = color;

    for(int i = 0; i < noPixels; i++){
        if(alphaGreaterThan(stroke.getPixel(i), 60)){
            int nearestOffset;
            bool nearestOffsetState = getNearestPixel(color, i, nearestOffset, 500);
            if(nearestOffsetState){

                auto targetPixel = color.getPixel(nearestOffset);
                auto coercedPixel = forceDimensionsTo4(targetPixel);
                assert(coercedPixel.size() == 4);
                img.insertPixel(coercedPixel, i);
            }
        }
    }
    return true;
}


int main(int argc, char *argv[]) {
    std::string st;
    std::string cl;
    std::string op = "opt.png";
    switch(argc){
        case 3:
            st = argv[1];
            cl = argv[2];
            break;
        case 4:
            st = argv[1];
            cl = argv[2];
            op = argv[3];
            break;
        default:
            std::cerr << "Usage: " << argv[0] << "stroke color [output=opt.png]\n";
            return 1;
    }

    auto stroke = Image(st);
    auto color = Image(cl);
    Image img = Image(1, 1, 4);
    if(replaceStrokePixels(stroke, color, img)) {
        img.saveImage(op);
    } else {
        throw std::runtime_error("Either image isn't the same dimensions or isn't PNG/RGBA");
    }
}
