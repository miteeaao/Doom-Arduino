#include <LiquidCrystal.h>
#include <math.h>

const int rs = 12, en = 11, d4 = 5, d5 = 4, d6 = 3, d7 = 2;
LiquidCrystal lcd(rs, en, d4, d5, d6, d7);

const int X_pin = 1;
const int Y_pin = 2;
const int SW_pin = 7;

const int FPS = 200;

char mapArray[16][20] = {
    "####################",
    "#________#________e#",
    "#_e_###__#_e_#####_#",
    "#__##___##_______#_#",
    "#__#__e________###_#",
    "#__###____e______#_#",
    "#______________e___#",
    "#___######_##______#",
    "#_e_#______#_______#",
    "#___#__P___#_______#",
    "#___#______#____e__#",
    "#___#______###_____#",
    "#___#__e___________#",
    "#__e__#_#_##########",
    "#_________##########",
    "####################"
};

float playerX = 8.5;
float playerY = 9.5;
float direction = 90;
const float fov = 60;
const float speed = 0.4;
const float turningSpeed = 8;
const float maxViewDistance = 8.0;
const float maxEnemyViewDistance = 2000.0;
int health = 100;
int kills = 0;
unsigned long lastDamageTime = 0;

bool curtainAnimation = true;
int curtainHeight = 0;
unsigned long curtainStartTime = 0;
const int curtainSpeed = 0;
const int maxHeight = 16;
int curtainLine = 0;
const int curtainDoomOffset[20] = {2, 4, 3, 2, 3, 4, 1, 2, 4, 3, 6, 8, 6, 7, 4, 8, 11, 6, 2, 4};
bool gameOver = false;

int shotgunFrame = 0;

bool timedFlip = true;
float timedStart = 0;
float timedInterval = 200;

const int fireDelay = 1000;
unsigned long lastFireTime = 0;
bool canFire = true;

bool pixelMatrix[16][20] = {0};

bool up, down, left, right;

float reloadStart;

struct Projectile {
  float x, y;
  float direction;
  float speed;
  bool active;
};

#define MAX_PROJECTILES 10
Projectile projectiles[MAX_PROJECTILES];

#define MAX_ENEMIES 10

struct Enemy {
  float x, y;
  bool active;
};

Enemy enemies[MAX_ENEMIES];

void setup() {
  pinMode(SW_pin, INPUT_PULLUP);
  Serial.begin(9600);
  lcd.begin(16, 2);
  ClearScreen();
  InitializeEnemies();
  curtainStartTime = millis();
}

void loop() {
  timedFlipFunktion();
  ClearScreen();
  if (curtainAnimation) {
    RunCurtainAnimation();
    Render3D();
    DrawShotgun();
  } else {
    controllerInput();
    UpdatePlayerMovement();
    UpdatePlayerStats();
    CheckForFire();
    ManageShotgunAnimations();
    UpdateProjectiles();
    Render3D();
    RenderEnemies();
    RenderProjectiles();
    DrawShotgun();
    Countdown();
  }
  RenderScreen();
  RenderUI();
  delay(1000 / FPS);
}

float CastRay(float angle) {
  float rayX = playerX;
  float rayY = playerY;

  float rayDirX = cos(angle);
  float rayDirY = sin(angle);

  float distance = 0.0;

  while (distance < maxViewDistance) {
    rayX += rayDirX * 0.1;
    rayY += rayDirY * 0.1;
    distance += 0.1;

    if (rayX < 0 || rayX >= 20 || rayY < 0 || rayY >= 16) {
      return maxViewDistance;
    }

    if (mapArray[(int)rayY][(int)rayX] == '#') {
      return distance;
    }
  }

  return maxViewDistance;
}

void Render3D() {
  for (int column = 0; column < 20; column++) {
    float rayAngle = direction + (column - 10) * (fov / 20);
    float rayDistance = CastRay(DegToRadian(rayAngle));

    if (rayDistance < maxViewDistance) {
      int wallHeight = max(1, (int)(8 / rayDistance));

      for (int row = 0; row < 16; row++) {
        if (row >= (8 - wallHeight) && row <= (8 + wallHeight)) {
          DrawPixel(column, row, true);
        } else {
          //DrawPixel(column, row, false);
        }
      }
    } else {
      for (int row = 0; row < 16; row++) {
        //DrawPixel(column, row, false);
      }
    }
  }
}

void RenderScreen() {
  for (int charY = 0; charY < 2; charY++) {
    for (int charX = 0; charX < 4; charX++) {
      byte customChar[8] = {0};

      for (int row = 0; row < 8; row++) {
        for (int col = 0; col < 5; col++) {
          int pixelX = charX * 5 + col;
          int pixelY = charY * 8 + row;
          if (pixelMatrix[pixelY][pixelX]) {
            customChar[row] |= (1 << (4 - col));
          }
        }
      }

      lcd.createChar(charY * 4 + charX, customChar);
      lcd.setCursor(charX, charY);
      lcd.write(charY * 4 + charX);
    }
  }
}

void ClearScreen() {
  for (int y = 0; y < 16; y++) {
    for (int x = 0; x < 20; x++) {
      pixelMatrix[y][x] = false;
    }
  }
}

void controllerInput() {
  if (analogRead(X_pin) < 212) up = true;
  else up = false;
  if (analogRead(X_pin) > 812) down = true;
  else down = false;
  if (analogRead(Y_pin) < 212) right = true;
  else right = false;
  if (analogRead(Y_pin) > 812) left = true;
  else left = false;
}

void UpdatePlayerMovement() {
  if (direction > 360) direction -= 360;
  if (direction < 0) direction += 360;

  if (right) direction += turningSpeed;
  if (left) direction -= turningSpeed;

  float newX = playerX;
  float newY = playerY;

  if (up) {
    newX += speed * cos(DegToRadian(direction));
    newY += speed * sin(DegToRadian(direction));
  }

  if (down) {
    newX -= speed * cos(DegToRadian(direction));
    newY -= speed * sin(DegToRadian(direction));
  }

  if (mapArray[(int)newY][(int)newX] != '#') {
    playerX = newX;
    playerY = newY;
  }
}

float DegToRadian(float degree) {
  return degree * PI / 180.0;
}

void FireProjectile() {
  for (int i = 0; i < MAX_PROJECTILES; i++) {
    if (!projectiles[i].active) {
      projectiles[i].x = playerX;
      projectiles[i].y = playerY;
      projectiles[i].direction = direction;
      projectiles[i].speed = 0.3;
      projectiles[i].active = true;
      break;
    }
  }
}

void UpdateProjectiles() {
  for (int i = 0; i < MAX_PROJECTILES; i++) {
    if (projectiles[i].active) {
      projectiles[i].x += projectiles[i].speed * cos(DegToRadian(projectiles[i].direction));
      projectiles[i].y += projectiles[i].speed * sin(DegToRadian(projectiles[i].direction));

      if (mapArray[(int)projectiles[i].y][(int)projectiles[i].x] == '#') {
        projectiles[i].active = false;
      }

      for (int j = 0; j < MAX_ENEMIES; j++) {
        if (enemies[j].active) {
          float dx = projectiles[i].x - enemies[j].x;
          float dy = projectiles[i].y - enemies[j].y;

          if (sqrt(dx * dx + dy * dy) < 0.5) {
            enemies[j].active = false;
            projectiles[i].active = false;
            kills++;
            break;
          }
        }
      }

      float dx = projectiles[i].x - playerX;
      float dy = projectiles[i].y - playerY;
      if (sqrt(dx * dx + dy * dy) > maxViewDistance) {
        projectiles[i].active = false;
      }
    }
  }
}

void RenderProjectiles() {
  for (int i = 0; i < MAX_PROJECTILES; i++) {
    if (projectiles[i].active) {
      float dx = projectiles[i].x - playerX;
      float dy = projectiles[i].y - playerY;
      float distance = sqrt(dx * dx + dy * dy);

      if (distance >= maxViewDistance) {
        projectiles[i].active = false;
        continue;
      }

      float angleToProjectile = atan2(dy, dx) * 180 / PI;

      float relativeAngle = angleToProjectile - direction;

      if (relativeAngle > 180) relativeAngle -= 360;
      if (relativeAngle < -180) relativeAngle += 360;

      if (fabs(relativeAngle) < fov / 2) {
        int screenX = (int)((relativeAngle + fov / 2) / fov * 20);
        int circleRadius = max(1, (int)(4 / distance));

        for (int row = 8 - circleRadius; row <= 8 + circleRadius; row++) {
          for (int col = screenX - circleRadius; col <= screenX + circleRadius; col++) {
            int dx = col - screenX;
            int dy = row - 8;
            if (dx * dx + dy * dy <= circleRadius * circleRadius) {
              if (row >= 0 && row < 16 && col >= 0 && col < 20) {
                DrawPixel(col, row, true);
              }
            }
          }
        }
      }
    }
  }
}

void UpdatePlayerStats(){
  return;
}

void CheckForFire(){
  bool buttonState = digitalRead(SW_pin);
  if (buttonState == LOW && canFire) {  
    FireProjectile();
    lastFireTime = millis();
    canFire = false;
  }

  if (buttonState == HIGH) {
    unsigned long currentTime = millis();
    if ((currentTime - lastFireTime) >= fireDelay) {
      canFire = true;
    }
  }
}

void RenderUI(){
  if(!gameOver){
    lcd.setCursor(4, 0);
    lcd.print("Health:");
    lcd.print(health);
    lcd.print("%         ");
    lcd.setCursor(4, 1);
    lcd.print("Kills:");
    lcd.print(kills);
    lcd.print("/");
    lcd.print(MAX_ENEMIES);
  }else{
    lcd.setCursor(4, 0);
    lcd.print("Game Over        ");
    lcd.setCursor(4, 1);
    if(kills >= 10)lcd.print("You won        ");
    if(health <= 0)lcd.print("You lost        ");
  }
}

void RunCurtainAnimation() {
  if (millis() - curtainStartTime >= curtainSpeed) {
    for (int i = 0; i < 20; i++) {
      for (int x = 0; x < 16; x++) {
        DrawPixel(i, x, true);
      }
    }

    for (int i = 0; i < 20; i++) {
      for (int x = 0; x < curtainHeight - curtainDoomOffset[i] - 5; x++) {
        DrawPixel(i, x, false);
      }
    }

    curtainHeight++;

    curtainStartTime = millis();

    if (curtainHeight >= maxHeight + 18) {
      curtainAnimation = false;
    }
  }
}

void DrawShotgun(){
  switch(shotgunFrame){
    case 1:
      DrawPixel(8, 15, true);
      DrawPixel(9, 15, true);
      DrawPixel(10, 15, true);
      DrawPixel(9, 14, true);
      DrawPixel(10, 14, true);
      DrawPixel(9, 13, true);
      DrawPixel(10, 13, true);
      break;
    case 2:
      DrawPixel(9, 15, true);
      DrawPixel(10, 15, true);
      DrawPixel(9, 14, true);
      DrawPixel(10, 14, true);
      break;
    case 3:
      DrawPixel(4, 15, true);
      DrawPixel(7, 5, true);
      DrawPixel(4, 15, true);
      DrawPixel(10, 11, true);
      DrawPixel(10, 12, true);
      for(int x = 5; x < 6 + 1; x++){
        for(int y = 11; y < 15 + 1; y++){
          DrawPixel(x, y, true);
        }
      }
      for(int x = 7; x < 8 + 1; x++){
        for(int y = 6; y < 15 + 1; y++){
          DrawPixel(x, y, true);
        }
      }
      for(int y = 9; y < 14 + 1; y++){
        DrawPixel(9, y, true);
      }
      break;
    case 4:
      DrawPixel(5, 2, true);
      DrawPixel(8, 12, true);
      DrawPixel(8, 13, true);
      for(int y = 10; y < 15 + 1; y++){
        DrawPixel(4, y, true);
      }
      for(int y = 7; y < 15 + 1; y++){
        DrawPixel(7, y, true);
      }
      for(int x = 5; x < 6 + 1; x++){
        for(int y = 3; y < 15 + 1; y++){
          DrawPixel(x, y, true);
        }
      }
      for(int x = 8; x < 9 + 1; x++){
        for(int y = 8; y < 11 + 1; y++){
          DrawPixel(x, y, true);
        }
      }
      break;
    case 5:
      DrawPixel(6, 9, true);
      DrawPixel(7, 9, true);
      for(int y = 10; y < 13 + 1; y++){
        DrawPixel(9, y, true);
      }
      for(int y = 7; y < 15 + 1; y++){
        DrawPixel(5, y, true);
      }
      for(int y = 6; y < 11 + 1; y++){
        DrawPixel(4, y, true);
      }
      for(int y = 5; y < 7 + 1; y++){
        DrawPixel(3, y, true);
      }
      for(int x = 6; x < 8 + 1; x++){
        for(int y = 10; y < 15 + 1; y++){
          DrawPixel(x, y, true);
        }
      }
      break;
    default:
      DrawPixel(8, 15, true);
      DrawPixel(9, 15, true);
      DrawPixel(10, 15, true);
      DrawPixel(11, 15, true);
      DrawPixel(8, 14, true);
      DrawPixel(9, 14, true);
      DrawPixel(10, 14, true);
      DrawPixel(9, 13, true);
      DrawPixel(10, 13, true);
      DrawPixel(9, 12, true);
      DrawPixel(10, 12, true);
  }
}

void timedFlipFunktion(){
  if(millis() - timedStart >= timedInterval){
    timedStart = millis();
    timedFlip = !timedFlip;
  }
}

void ManageShotgunAnimations(){
  if((up || down) && canFire){
    if(timedFlip)shotgunFrame = 1;
    if(!timedFlip)shotgunFrame = 0;
  }else if(!canFire){
    float fireStep = fireDelay / 6.0;
    unsigned long elapsedTime = millis() - lastFireTime;

    if (elapsedTime > fireStep * 0) shotgunFrame = 2;
    if (elapsedTime > fireStep * 1) shotgunFrame = 3;
    if (elapsedTime > fireStep * 2) shotgunFrame = 4;
    if (elapsedTime > fireStep * 3) shotgunFrame = 5;
    if (elapsedTime > fireStep * 4) shotgunFrame = 4;
    if (elapsedTime > fireStep * 5) shotgunFrame = 3;
  }else{
    shotgunFrame = 0;
  }
}

void DrawPixel(int x, int y, bool state) {
  if (x >= 0 && x < 20 && y >= 0 && y < 16) {
   pixelMatrix[y][x] = state;
  }
}

void InitializeEnemies() {
  enemies[0] = {18.5, 2.5, true};
  enemies[1] = {2.5, 3.5, true};
  enemies[2] = {10.5, 3.5, true};
  enemies[3] = {6.5, 4.5, true};
  enemies[4] = {8.5, 5.5, true};
  enemies[5] = {14.5, 6.5, true};
  enemies[6] = {2.5, 8.5, true};
  enemies[7] = {14.5, 10.5, true};
  enemies[8] = {6.5, 12.5, true};
  enemies[9] = {2.5, 13.5, true};
}


void RenderEnemies() {
  for (int i = 0; i < MAX_ENEMIES; i++) {
    if (enemies[i].active) {
      float dx = enemies[i].x - playerX;
      float dy = enemies[i].y - playerY;
      float distance = sqrt(dx * dx + dy * dy);

      if (distance >= maxEnemyViewDistance) {
        enemies[i].active = false;
        continue;
      }

      float angleToEnemy = atan2(dy, dx) * 180 / PI;
      float relativeAngle = angleToEnemy - direction;

      if (relativeAngle > 180) relativeAngle -= 360;
      if (relativeAngle < -180) relativeAngle += 360;

      if (fabs(relativeAngle) < fov / 2) {
        int screenX = (int)((relativeAngle + fov / 2) / fov * 20);
        int size = max(1, (int)(4 / distance));

        for (int row = -size; row <= size; row++) {
          for (int col = -size + abs(row); col <= size - abs(row); col++) {
            int pixelX = screenX + col;
            int pixelY = 8 + row;

            if (pixelX >= 0 && pixelX < 20 && pixelY >= 0 && pixelY < 16) {
              if (pixelMatrix[pixelY][pixelX] == true) {
                DrawPixel(pixelX, pixelY, false);
                pixelMatrix[pixelY][pixelX] = false;
              } else {
                DrawPixel(pixelX, pixelY, true);
                pixelMatrix[pixelY][pixelX] = true;
              }
            }
          }
        }
      }
    }
  }
}

void Countdown(){
  if (millis() - lastDamageTime >= 1000) {
    lastDamageTime = millis();
    if (health > 0) {
      health--;
    }
  }
  if(health <= 0){
    gameOver = true;
  }
  if(kills >= 10){
    gameOver = true;
  }
}