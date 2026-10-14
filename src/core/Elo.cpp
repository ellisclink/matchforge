// Elo.cpp: the Elo formulas declared in Elo.h.
#include "core/Elo.h"

#include <cmath>

namespace matchforge::core {

double expectedScore(double rating, double opponentRating) {
    // Dividing by 400 is part of the standard definition: it sets the scale so that a
    // 400-point gap gives the stronger side 10-to-1 odds.
    return 1.0 / (1.0 + std::pow(10.0, (opponentRating - rating) / 400.0));
}

int ratingDelta(double rating, double opponentRating, double score, int kFactor) {
    const double change = kFactor * (score - expectedScore(rating, opponentRating));
    // Round to the nearest whole point, so ratings stay integers players can read.
    return static_cast<int>(std::lround(change));
}

}  // namespace matchforge::core
