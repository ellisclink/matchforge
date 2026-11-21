// Elo.h: the Elo rating system, used to measure player skill.
//
// Every player has a number (the rating). When two players meet, the system predicts
// who should win from the rating gap. After the game, each rating moves by an amount
// proportional to how surprising the result was: beating a much stronger player gains
// a lot, beating a much weaker one gains little.
#pragma once

namespace matchforge::core {

// Rating given to a new player. 1500 is the usual Elo starting point.
inline constexpr int kDefaultRating = 1500;

// How far one game can move a rating. 32 is a common choice for active online games:
// big enough that new players find their level quickly, small enough to stay stable.
inline constexpr int kDefaultKFactor = 32;

// The result of a game from one side's point of view.
inline constexpr double kWin = 1.0;
inline constexpr double kDraw = 0.5;
inline constexpr double kLoss = 0.0;

// The chance (0 to 1) that a side rated `rating` beats a side rated `opponentRating`.
// Formula: 1 / (1 + 10^((opponent - rating) / 400)). A 400-point gap means about 91%.
double expectedScore(double rating, double opponentRating);

// How much a rating changes after one game: K * (actual score - expected score), rounded.
// For a team game, pass the two teams' average ratings; every player on the team gets
// the same change.
int ratingDelta(double rating, double opponentRating, double score, int kFactor = kDefaultKFactor);

}  // namespace matchforge::core
