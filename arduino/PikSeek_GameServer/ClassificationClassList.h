#ifndef __CLASSIFICATIONCLASSLIST_H__
#define __CLASSIFICATIONCLASSLIST_H__

struct ClassificationDetectionItem {
    uint8_t index;
    const char* imgclassName;
    uint8_t filter;
};

// List of objects the pre-trained model is capable of recognizing
// Index number is fixed and hard-coded from training
// Set the filter value to 0 to ignore any recognized objects
ClassificationDetectionItem imgclassItemList[2] = {
    {0, "107", 1},
    {1, "not 107", 1},
};

ClassificationDetectionItem imgclassMobileNetV2ItemList[5] = {
    {0, "Daisy",     1},
    {1, "Lavender",  1},
    {2, "Lily",      1},
    {3, "Rose",      1},
    {4, "Sunflower", 1},
};

#endif