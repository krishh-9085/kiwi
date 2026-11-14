#pragma once
#include <Arduino.h>

enum HungerState {
  HUNGER_SATISFIED = 0,
  HUNGER_HUNGRY    = 1,
  HUNGER_ANGRY     = 2
};

enum MealType {
  MEAL_GENERIC   = 0,
  MEAL_BREAKFAST = 1,
  MEAL_LUNCH     = 2,
  MEAL_DINNER    = 3
};

void initHungerEngine();
void checkHungerBehavior();
bool isKiwiHungry();
bool isKiwiAngry();
HungerState getHungerState();
int32_t getHungerRemainingSec();
MealType getCurrentMeal();
const char *getCurrentMealName();

void triggerHunger(MealType meal = MEAL_GENERIC);
bool feedKiwi();
bool reconcileKiwi();
void handleAngryRejection();
void handleAngryFeedAttempt();

// Alternating cheek tap combo tracking (Left -> Right -> Left -> Right)
void recordCheekTapForCombo(uint8_t pin, uint32_t now);
bool checkCheekComboLRLR();
