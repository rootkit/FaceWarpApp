//
//  triangulator.cpp
//  FaceWarpApp
//
//  Created by Thomas Nickson on 26/09/2015.
//  Copyright © 2015 Phi Research. All rights reserved.
//

#import "triangulator.h"
#import "Clarkson-Delaunay.h"
#include <CoreGraphics/CoreGraphics.h>
#include <stdlib.h>
#include <vector>
// factorials (multiplies example)
#include <iostream>     // std::cout
#include <functional>   // std::multiplies
#include <numeric>      // std::partial_sum
#include <set>

std::vector<PhiTriangle> infaceTri {{ 0, 36, 17},{36, 18, 17},{36, 37, 18},{37, 19, 18},{37, 38, 19},{38, 20, 19},{38, 39, 20},{39, 21, 20},{36, 41, 37},{41, 40, 37},{40, 38, 37},{40, 39, 38},{39, 27, 21},{27, 22, 21},{27, 42, 22},{42, 23, 22},{42, 43, 23},{43, 24, 23},{43, 44, 24},{44, 25, 24},{44, 45, 25},{45, 26, 25},{45, 16, 26},{42, 47, 43},{47, 44, 43},{47, 46, 44},{46, 45, 44},{39, 28, 27},{28, 42, 27},{32, 33, 30},{33, 34, 30},{31, 30, 32},{31, 30, 29},{34, 35, 30},{35, 29, 30},{35, 28, 29},{31, 29, 28},{ 0,  1, 36},{39, 31, 28},{35, 42, 28},{15, 16, 45},{40, 31, 39},{35, 47, 42},{ 1, 41, 36},{ 1, 40, 41},{15, 45, 46},{15, 46, 47},{35, 15, 47},{ 1, 31, 40},{ 1,  2, 31},{35, 14, 15},{ 2, 48, 31},{ 3, 48,  2},{ 4, 48,  3},{54, 14, 35},{54, 13, 14},{12, 13, 54},{ 4,  5, 48},{ 5, 59, 48},{11, 12, 54},{55, 11, 54},{10, 11, 55},{56, 10, 55},{ 9, 10, 56},{ 5,  6, 59},{ 6, 58, 59},{ 6,  7, 58},{ 7, 57, 58},{ 7,  8, 57},{57,  9, 56},{ 8,  9, 57},{48, 49, 31},{53, 54, 35},{49, 50, 31},{52, 53, 35},{50, 32, 31},{52, 35, 34},{50, 51, 32},{51, 52, 34},{51, 34, 33},{51, 33, 32},{48, 60, 49},{59, 60, 48},{60, 67, 61},{64, 54, 53},{55, 54, 64},{65, 64, 63},{67, 62, 61},{65, 63, 62},{67, 66, 62},{66, 65, 62},{51, 52, 63},{61, 62, 51},{60, 61, 49},{61, 50, 49},{63, 64, 53},{63, 53, 52},{61, 51, 50},{51, 62, 63},{59, 67, 60},{59, 58, 67},{58, 57, 67},{57, 66, 67},{57, 65, 66},{57, 56, 65},{65, 55, 56},{55, 64, 65}};

PhiTriangle operator +(PhiTriangle tri, int offset) {
    return PhiTriangle{tri.p0 + offset, tri.p1 + offset, tri.p2 + offset};
}

bool triInFaceRange(PhiTriangle tri, int incL, int excU) {
    bool flag = true; // If any of the following are false, sets to false
    flag &= ((tri.p0 >= incL) && (tri.p0 < excU));
    flag &= ((tri.p1 >= incL) && (tri.p1 < excU));
    flag &= ((tri.p2 >= incL) && (tri.p2 < excU));
    return flag;
}

std::vector<PhiTriangle> pruneTriangulation(int numFaces, int edgesOffset, const std::vector<PhiTriangle> & delaunay) {
    std::vector<PhiTriangle> possibleTriang = delaunay;
    // For each face, calculate the lower and upper indices for points entirely inside that face.
    // Append a triangle to localGoodTriang if it doesn't have three points inside that given face.
    // Assign localGoodTriang to goodTriang and repeat for the next face.
    
    for (int faceIdx = 0; faceIdx < numFaces; ++faceIdx) {
        std::vector<PhiTriangle> localTriang;
        int faceLower = edgesOffset + faceIdx * 68;      // Lower (inclusive) bound
        int faceUpper = edgesOffset + faceIdx * 68 + 68; // Upper (exclusive) bound
        for (PhiTriangle tri : possibleTriang) {
            if (!triInFaceRange(tri, faceLower, faceUpper)) {
                localTriang.push_back(tri);
            }
        }
        possibleTriang = localTriang;
    }
    return possibleTriang;
}



// Needs c linkage to be imported to Swift
extern "C" {
PhiTriangle * unsafeTidyIndices(CGPoint * edgesLandMarks, int numEdges, int numFaces, int * nTris) {
    // CALLER TO FREE RETURN VALUE
    int numVertices;
    int dim = 2;
    int numPoints = numEdges + numFaces * 68;
    // Result is an numVertices array of unsigned integers, in row major form representing a matrix (numVertices / 3) x 3
    // Second argument, 'factor', is how much to mutiply the floats by such that when they are cast to ints they are seperable. Must be big if range is normalised.
    //
    
    // we cast edgesLandMarks from cgpoint to cgfloat because BuildTriangleIndexList expects that
    CGFloat * edgesLandMarksFloat = (CGFloat * ) edgesLandMarks;
    int * intEdges = (int * ) malloc(numPoints * 2 * sizeof(int));
    for (int idx = 0; idx < 2 * numPoints; ++idx) {
        intEdges[idx] = int(edgesLandMarksFloat[idx]);
    }
    unsigned int * result = BuildTriangleIndexList(intEdges, 0, numPoints, dim, 1, &numVertices);
    free(intEdges);
    if (numVertices == 0) {
        exit(1);
    }
    
    // cast to become triangles...
    PhiTriangle * triResults = (PhiTriangle *)(result);
    
    std::vector<CGPoint> edge_points(edgesLandMarks, edgesLandMarks + numEdges);
    std::vector<CGPoint> landmarks(edgesLandMarks + numEdges, edgesLandMarks + numEdges + numFaces * 68);
    std::vector<PhiTriangle> triVector(triResults, triResults + numVertices / 3);

    // Tidy up the triangles by removing all of the inface ones
    std::vector<PhiTriangle> tidied = pruneTriangulation(numFaces, numEdges, triVector);
    
    // For each face, append the correctly incremented indexes of the inface triangulation
    for (int faceIdx = 0; faceIdx < numFaces; ++faceIdx) {
        int offset = faceIdx * 68 + numEdges;
        for (PhiTriangle tri : infaceTri) {
            tidied.push_back(tri + offset);
        }
    }

    // Now for dirty bit.
    
    // Set ntris to be the size of 'tidied'
    *nTris = tidied.size();
    //Allocate an array of PhiTriangle and copy vector into it
    PhiTriangle * unsafeResult = (PhiTriangle *) malloc(tidied.size() * sizeof(PhiTriangle));
    for (int tridX = 0; tridX < tidied.size(); tridX++) {
        unsafeResult[tridX] = tidied.at(tridX);
    }
    return unsafeResult;
}
}