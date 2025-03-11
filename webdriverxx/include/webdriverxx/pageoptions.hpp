#pragma once

#include "utils.hpp"
#include "json.hpp"

using json = nlohmann::json;

namespace webdriverxx {
    class PageOptions {
        private:
            std::optional<ORIENTATION> orientation_;
            std::optional<bool> background_, shrinkToFit_;
            std::optional<float> pageHeight_, pageWidth_, pageScale_;
            std::optional<float> marginTop_, marginBottom_, marginLeft_, marginRight_;
            std::optional<std::vector<std::string>> pageRanges_;

        public:
            PageOptions  &background(bool value) {  background_ = value; return *this; }
            PageOptions &shrinkToFit(bool value) { shrinkToFit_ = value; return *this; }

            PageOptions  &pageWidth(float value) {  pageWidth_ = value; return *this; }
            PageOptions  &pageScale(float value) {  pageScale_ = value; return *this; }
            PageOptions &pageHeight(float value) { pageHeight_ = value; return *this; }

            PageOptions    &marginTop(float value) {    marginTop_ = value; return *this; }
            PageOptions   &marginLeft(float value) {   marginLeft_ = value; return *this; }
            PageOptions  &marginRight(float value) {  marginRight_ = value; return *this; }
            PageOptions &marginBottom(float value) { marginBottom_ = value; return *this; }

            PageOptions &orientation(ORIENTATION &value) { 
                orientation_ = value; return *this; 
            }

            PageOptions &pageRanges(const std::vector<std::string> &value) { 
                pageRanges_ = value; return *this; 
            }

            operator json() const {
                json object;

                if ( background_) object["background"] = *background_;
                if (shrinkToFit_) object["shrinkToFit"] = *shrinkToFit_;

                if (   pageWidth_) object[   "pageWidth"] =    *pageWidth_;
                if (   pageScale_) object[   "pageScale"] =    *pageScale_;
                if (  pageHeight_) object[  "pageHeight"] =   *pageHeight_;

                if (   marginTop_) object[   "marginTop"] =    *marginTop_;
                if (  marginLeft_) object[  "marginLeft"] =   *marginLeft_;
                if ( marginRight_) object[ "marginRight"] =  *marginRight_;
                if (marginBottom_) object["marginBottom"] = *marginBottom_;

                if (orientation_) object["orientation"] = (
                        *orientation_ == ORIENTATION::LANDSCAPE? 
                        "landscape": "portrait"
                );

                if (pageRanges_) {
                    json ranges = json::array();
                    for (const std::string &str: *pageRanges_)
                        ranges.push_back(str);
                    object["pageRanges"] = ranges;
                }

                return object;
            }
    };
}
