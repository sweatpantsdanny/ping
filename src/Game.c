#define SINGLE

// PREPROCESSOR DIRECTIVES :
// SINGLE   : Use this configuration if debugging with one board. CreateGame doesn't
//             wait to read data from the client.
// MULTI    : Primary mode allows both boards to communicate with each other.

/*
 * Game.c
 *
 *  Created     : 4/2/2019
 *  Last Edit   : 4/2/2019
 *
 *  UPDATES     :
 *  4/2/2019    : Initialized threads and game functions.
 *
 *  TODO        :
 *  ~ Update player displacement/ready on initialization in JoinGame.
 *  ~ Does the player.acknowledge need to be updated in JoinGame?
 *  ~ Port number may be incorrect in the header configurations
 *  ~ Does it matter what the CreateGame sends back to client when acknowledging?
 *  ~ Initialize board in CreateGame and JoinGame
 *  ~ What is the difference between gamestate.gameDone and gamestate.winner?
 *  ~ Finish moveball collision logic
 *  ~ Possibly need a semaphore for the player center data.
 *
 */

#include "Game.h"

// ======================       GLOBALS            ==========================
GameState_t gamestate;
uint8_t playerCount = 0;
uint8_t ballCount = 0;

// ======================      SEMAPHORES          ==========================
semaphore_t CLIENT_PLAYERDATA_SEMAPHORE;

// ======================     GAME FUNCTIONS       ==========================

// initialize interrupts for input buttons B0, B1, B2, B3
// on ports 4 and 5.
void buttons_init(void)
{
    // B0 = P4.4, B1 = P4.5
    P4->DIR &= ~(BIT4 | BIT5);
    P4->IFG &= ~(BIT4 | BIT5);      // P4.4 IFG cleared
    P4->IE  |= BIT4 | BIT5;         // Enable interrupt on P4.4
    P4->IES |= BIT4 | BIT5;         // high-to-low transition
    P4->REN |= BIT4 | BIT5;         // Pull-up resister
    P4->OUT |= BIT4 | BIT5;         // Sets res to pull-up
    NVIC_EnableIRQ(PORT4_IRQn);     // enable interrupts on PORT4
    __NVIC_SetPriority(PORT4_IRQn, 0);

    // B2 = P5.4, B3 = P5.5
    P5->DIR &= ~(BIT4 | BIT5);
    P5->IFG &= ~(BIT4 | BIT5);      // P4.4 IFG cleared
    P5->IE  |= BIT4 | BIT5;         // Enable interrupt on P4.4
    P5->IES |= BIT4 | BIT5;         // high-to-low transition
    P5->REN |= BIT4 | BIT5;         // Pull-up resister
    P5->OUT |= BIT4 | BIT5;         // Sets res to pull-up
    NVIC_EnableIRQ(PORT5_IRQn);     // enable interrupts on PORT4
    NVIC_SetPriority(PORT5_IRQn, 0);
}

// Any animations or text used for the main menu is displayed with this function
void writeMainMenu( uint16_t Color )
{
    LCD_Text(0, 0, "B0 -> HOST", Color);
    LCD_Text(0, 16, "B2 -> HOST", Color);
    srand(time(0));  // generated for later use
}

/*
 * Returns either Host or Client depending on button press
 */
playerType GetPlayerRole()
{

}

/*
 * Draw players given center X center coordinate
 */
void DrawPlayer(GeneralPlayerInfo_t * player)
{
    int16_t yCenter = 0;

    // determine the player's Y offset based on whether it should be on the top or bottom of the screen
    if (player->position == BOTTOM) yCenter = ARENA_MAX_Y - PADDLE_WID_D2 - PADDLE_OFFSET;
    if (player->position == TOP)    yCenter = ARENA_MIN_Y + PADDLE_WID_D2 + PADDLE_OFFSET;

    LCD_DrawRectangle(player->currentCenter - PADDLE_LEN_D2, player->currentCenter + PADDLE_LEN_D2,
                      yCenter - PADDLE_WID_D2, yCenter + PADDLE_WID_D2, player->color);

}

/*
 * Updates player's paddle based on current and new center
 *
 * NOTE: Only updates the portion of the paddle that changes on the screen. All
 *      common areas are ignored in order to save processor time.
 */
void UpdatePlayerOnScreen(PrevPlayer_t * prevPlayerIn, GeneralPlayerInfo_t * outPlayer)
{
    int16_t yCenter = 0;

    // only update the player if it's center moved
    if ( prevPlayerIn->Center != outPlayer->currentCenter )
    {
        // determine the player's Y offset based on whether it should be on the top or bottom of the screen
        if (outPlayer->position == BOTTOM) yCenter = ARENA_MAX_Y - PADDLE_WID_D2 - PADDLE_OFFSET;
        if (outPlayer->position == TOP)    yCenter = ARENA_MIN_Y + PADDLE_WID_D2 + PADDLE_OFFSET;

        // Calculate the COMMON area's length
        int16_t center_diff = abs(prevPlayerIn->Center - outPlayer->currentCenter);

        int16_t starting_old_data_window;
        int16_t starting_new_data_window;

        // If the old player data is to the left, erase that data.
        if ( prevPlayerIn->Center - outPlayer->currentCenter < 0 )
        {
            // Calulate the starting x position for the UNCOMMON OLD player's area on the LEFT side
            starting_old_data_window = prevPlayerIn->Center - PADDLE_LEN_D2;

            // Calculate the starting x position for the UNCOMMON NEW player's area on the RIGHT side
            starting_new_data_window = outPlayer->currentCenter + PADDLE_LEN_D2 - center_diff;
        }

        // If the old player data is to the right, erase that data.
        else if ( prevPlayerIn->Center - outPlayer->currentCenter > 0 )
        {
            // Calulate the starting x position for the UNCOMMON OLD player's area on the RIGHT side
            starting_old_data_window = prevPlayerIn->Center + PADDLE_LEN_D2 - center_diff;

            // Calculate the starting x position for the UNCOMMON NEW player's area on the LEFT side
            starting_new_data_window = outPlayer->currentCenter - PADDLE_LEN_D2;
        }

        // erase the UNCOMMON old player position first
        LCD_DrawRectangle(starting_old_data_window, starting_old_data_window + center_diff,
                          yCenter - PADDLE_WID_D2, yCenter + PADDLE_WID_D2, BACK_COLOR);

        // draw the UNCOMMON updated player position
        LCD_DrawRectangle(starting_new_data_window, starting_new_data_window + center_diff,
                          yCenter - PADDLE_WID_D2, yCenter + PADDLE_WID_D2, outPlayer->color);

    }
}

/*
 * Function updates ball position on screen
 */
void UpdateBallOnScreen(PrevBall_t * previousBall, Ball_t * currentBall, uint16_t outColor)
{
    // erase the old ball first
    LCD_DrawRectangle(previousBall->CenterX, previousBall->CenterX + BALL_SIZE,
                      previousBall->CenterY, previousBall->CenterY + BALL_SIZE, BACK_COLOR);

    // draw the new ball next
    LCD_DrawRectangle(currentBall->currentCenterX, currentBall->currentCenterX + BALL_SIZE,
                      currentBall->currentCenterY, currentBall->currentCenterY + BALL_SIZE, outColor);

}

/*
 * Initializes and prints initial game state
 */
void InitBoardState()
{
    // initialize balls
    Ball_t * tempBall;
    for (int i = 0; i < MAX_NUM_OF_BALLS; i++)
    {
        tempBall = &gamestate.balls[i];
        tempBall->alive = 0;
        tempBall->color = LCD_WHITE;
        tempBall->currentCenterX = 0;
        tempBall->currentCenterY = 0;
    }

    // initialize game states
    gamestate.gameDone = 0;
    gamestate.numberOfBalls = 0;
    gamestate.winner = 0;

    // draw the map boundaries
    LCD_Clear(BACK_COLOR);
    LCD_DrawRectangle(ARENA_MIN_X-1, ARENA_MIN_X, ARENA_MIN_Y, ARENA_MAX_Y, LCD_WHITE);
    LCD_DrawRectangle(ARENA_MAX_X, ARENA_MAX_X+1, ARENA_MIN_Y, ARENA_MAX_Y, LCD_WHITE);

    char score_str[3] = {0, 0, 0};

    // draw the current score of the players
    snprintf( score_str, 2, "%d", gamestate.overallScores[0] );
    LCD_Text(0, MAX_SCREEN_Y - 16 - 1, (uint8_t*)score_str, PLAYER_RED);

    snprintf( score_str, 2, "%d", gamestate.overallScores[1] );
    LCD_Text(0, 0, (uint8_t*)score_str, PLAYER_BLUE);
}

// ====================== HOST HOST HOST HOST HOST ==========================
/*
 * SUMMARY: Thread for the host to create a game.
 *
 * DESCRIPTION:
 * 1. Initialize players
 * 2. Establish connection with client (use LED to indicate WiFi conn.)
 * 3. Try to receive packet from the client. (while)
 * 4. Acknowledge client once the client joins.
 * 5. Initialize the board (draw arena, players, scores)
 * 6. Add GenerateBall, DrawObjects, ReadJoystickHost, SendDataToClient
 *      ReceiveDataFromClient, MoveLEDs (low priority), Idle
 * 7. Kill self.
 *
 * NOTE: Player initializations will need to be updated to count up to 3 if
 *       the game is updated to be a 4 player game.
 */
void CreateGame()
{
    // declare variables
    GeneralPlayerInfo_t * tempPlayer;

    // 1. Initialize players' general parameters
    // player 1 initializations...
    tempPlayer = &gamestate.players[0];
    tempPlayer->color = PLAYER_RED;
    tempPlayer->currentCenter = MAX_SCREEN_X / 2;
    tempPlayer->position = BOTTOM;
    playerCount++;

    // player 2 initializations...
    tempPlayer = &gamestate.players[1];
    tempPlayer->color = PLAYER_BLUE;
    tempPlayer->currentCenter = MAX_SCREEN_X / 2;
    tempPlayer->position = TOP;

    // initialize number of points for the players. Don't include this
    // in the initBoard function so that we can use that function to
    // restart the game without clearing the scores.
    gamestate.overallScores[0] = 0;
    gamestate.overallScores[1] = 0;

    // 2. Establish connection with client
    P2->OUT &= ~(BIT0 | BIT1 | BIT2); // initialize led's off
    P2->DIR |= (BIT0 | BIT1 | BIT2); // set R.G.B direction

#ifdef MULTI
    // 3. Try to receive packet from the client until return SUCCESS == 0
    int test = 5;
    ReceiveData((_u8*)&test, sizeof(test));
    while( ReceiveData((_u8*)&gamestate.player, sizeof(gamestate.player)) );

    // 4. Acknowledge client by telling them they joined the game.
    gamestate.player.joined = true;
    SendData( (_u8*)&gamestate.player.joined ,
              gamestate.player.IP_address    ,
              sizeof(gamestate.player.joined) );
#endif

    GREEN_ON; // use LED to indicate WiFi connection as HOST

    // 5. Initialize the board (draw arena, players, scores)
    InitBoardState();

    // 6. Add GenerateBall, DrawObjects, ReadJoystickHost, SendDataToClient
    //      ReceiveDataFromClient, MoveLEDs (low priority), Idle
    G8RTOS_AddThread( &GenerateBall, DEFAULT_PRIORITY, 0xFFFFFFFF,          "GENERATE_BALL___" );
    G8RTOS_AddThread( &DrawObjects, 10, 0xFFFFFFFF,                         "DRAW_OBJECTS____" );
    G8RTOS_AddThread( &ReadJoystickHost, DEFAULT_PRIORITY, 0xFFFFFFFF,      "READ_JOYSTICK___" );
    G8RTOS_AddThread( &SendDataToClient, DEFAULT_PRIORITY, 0xFFFFFFFF,      "SEND_DATA_______" );
    G8RTOS_AddThread( &ReceiveDataFromClient, DEFAULT_PRIORITY, 0xFFFFFFFF, "RECEIVE_DATA____" );
    G8RTOS_AddThread( &MoveLEDs, 254, 0xFFFFFFFF,                           "MOVE_LEDS_______" );
    G8RTOS_AddThread( &IdleThread, 255, 0xFFFFFFFF,                         "IDLE____________" );

    // 7. Kill self.
    G8RTOS_KillSelf();
}

/*
 * Thread that sends game state to client
 */
void SendDataToClient()
{
    while(1)
    {

    }
}

/*
 * Thread that receives UDP packets from client
 */
void ReceiveDataFromClient()
{
    while(1)
    {

    }
}

/*
 * Generate Ball thread
 */
// Adds another ball thread. Sleeps proportional to the number of balls
// currently in play.
void GenerateBall()
{
    while(1)
    {
        // wake up the next available ball in the array if
        // the max number of balls have not been generated.
        if ( ballCount < MAX_NUM_OF_BALLS )
        {
            G8RTOS_AddThread(&MoveBall, DEFAULT_PRIORITY, 0xFFFFFFFF, "MOVE_BALL_______");
            ballCount++;
        }
        sleep(ballCount * BALL_GEN_SLEEP);
    }
}

/*
 * Thread to read host's joystick
 */
void ReadJoystickHost()
{
    int16_t avg = 0;
    int16_t joystick_x = 0;
    int16_t joystick_y = 0;
    int16_t displacement = 0;

    while(1)
    {
        // moving average of the joystick inputs
        GetJoystickCoordinates(&joystick_x, &joystick_y);
        avg = (avg + joystick_x + JOYSTICK_BIAS) >> 1;

        // use the joystick controls to increase and decrease
        // the player velocity
        switch(avg) {
        case -10000 ... -6000   : displacement = 10;   break;
        case -5999 ... -4000    : displacement = 7;    break;
        case -3999 ... -2000    : displacement = 5;    break;
        case -1999 ... -750     : displacement = 3;    break;
        case -749  ... 750      : displacement = 0;     break;
        case 751   ... 2000     : displacement = -3;     break;
        case 2001  ... 4000     : displacement = -5;     break;
        case 4001  ... 6000     : displacement = -7;     break;
        case 6001  ... 10000    : displacement = -10;    break;
        default                 : displacement = 0;     break;
        }

        sleep(10);  // sleep before updating host's position to make it fair for client

        // move the player's center
        gamestate.players[0].currentCenter += displacement;

        // player center is too far to the left - limit it.
        if ( gamestate.players[0].currentCenter - PADDLE_LEN_D2 <= ARENA_MIN_X )
        {
            gamestate.players[0].currentCenter = ARENA_MIN_X + PADDLE_LEN_D2 + 1;
        }

        // player center is too far to the right - limit it.
        else if ( gamestate.players[0].currentCenter + PADDLE_LEN_D2 > ARENA_MAX_X )
        {
            gamestate.players[0].currentCenter = ARENA_MAX_X - PADDLE_LEN_D2 - 1;
        }

    }
}

/*
 * SUMMARY: Thread to move a single ball
 *
 * DESCRIPTION: This thread is responsible for moving the ball at
 *              a constant velocity, handling collisions, and
 *              incrementing the score for whichever player scored.
 */
void MoveBall()
{
    // initialize the ball properties for the ball
    // that will be used in this thread.
    Ball_t * ball;
    Ball_t new_ball; // used in while loop..
    for (int i = 0; i < MAX_NUM_OF_BALLS; i++)
    {
        ball = &gamestate.balls[i];
        if ( ball->alive == 0 )
        {
            ball->alive = 1;
            ball->color = LCD_WHITE;
            ball->currentCenterX = BALL_SIZE * (rand() % (ARENA_MAX_X/BALL_SIZE)) + ARENA_MIN_X;
            ball->currentCenterY = BALL_SIZE * (rand() % (ARENA_MAX_Y/BALL_SIZE));
            ballCount++;
            break;
        }
    }

    int16_t xvel = rand() % MAX_BALL_SPEED + 1;
    int16_t yvel = rand() % MAX_BALL_SPEED + 1;

    while(1)
    {
        // adjust velocity based on accelerometer here..

        // collision detection if reaching the left or right side borders
        // UPDATE TO USE MINKOWSKI ALGORITHM.
        if ( ball->currentCenterX + BALL_SIZE >= ARENA_MAX_X
                || ball->currentCenterX <= ARENA_MIN_X      )
        {
            xvel *= -1; // go the opposite direction
        }

        // collision detection and color change if the ball hits a player

        // player scores

        // attempt to move ball in current direction
        new_ball.currentCenterX += xvel;
        new_ball.currentCenterY += yvel;

        // update ball on screen
        // UpdateBallOnScreen(ball, &new_ball, new_ball.color);
        *ball = new_ball;
        sleep(35);
    }
}

/*
 * End of game for the host
 */
void EndOfGameHost()
{
    while(1)
    {

    }
}

// ==================== CLIENT CLIENT CLIENT CLIENT ==========================
/*
 * SUMMARY: Thread for client to join game.
 *
 * DESCRIPTION:
 * 1. Set initial SpecificPlayerInfo_t strict attributes ( getLocalIP() ).
 * 2. Send player into the host.
 * 3. Wait for server response.
 * 4. If you've joined the game, acknowledge you've joined to the host
 *      and show connection through LED.
 * 5. Initialize board state, semaphores, and add the following threads..
 *      ReadJoystickClient, SendDataToHost, ReceiveDataFromHost, DrawObjects,
 *      MoveLEDs, Idle
 * 6. Kill self.
 */
void JoinGame()
{
    // 1. Set initial SpecificPlayerInfo_t strict attributes ( getLocalIP() ).
    SpecificPlayerInfo_t player;
    player.IP_address = getLocalIP();
    player.acknowledge = false;
    player.displacement = 0;
    player.joined = false;
    player.playerNumber = playerCount;
    player.ready = false;
    playerCount++;

    // 2. Send player data into the host.
    SendData( (_u8*)&player, player.IP_address, sizeof(player) );

    // hardware initialization
    P2->OUT &= ~(BIT0 | BIT1 | BIT2); // initialize led's off
    P2->DIR |= (BIT0 | BIT1 | BIT2); // set R.G.B direction

    // 3. Wait for server response showing that it acknowledges the new player
    while( ReceiveData( (_u8*)&player.joined, sizeof(player.joined)) );

    // 4. If you've joined the game, acknowledge you've joined to the host
    //      and show connection through LED.
    BLUE_ON;

    // 5. Initialize board state, semaphores, and add the following threads..
    //      ReadJoystickClient, SendDataToHost, ReceiveDataFromHost, DrawObjects,
    //      MoveLEDs, Idle
    InitBoardState();
    G8RTOS_InitSemaphore(&CLIENT_PLAYERDATA_SEMAPHORE, 1);

    G8RTOS_AddThread( &ReadJoystickClient, DEFAULT_PRIORITY, 0xFFFFFFFF,    "READ_JOYSTICK___" );
    G8RTOS_AddThread( &SendDataToHost, DEFAULT_PRIORITY, 0xFFFFFFFF,        "SEND_DATA_______" );
    G8RTOS_AddThread( &ReceiveDataFromHost, DEFAULT_PRIORITY, 0xFFFFFFFF,   "RECEIVE_DATA____" );
    G8RTOS_AddThread( &DrawObjects, DEFAULT_PRIORITY, 0xFFFFFFFF,           "DRAW_OBJECTS____" );
    G8RTOS_AddThread( &MoveLEDs, 244, 0xFFFFFFFF,                           "MOVE_LEDS_______" );
    G8RTOS_AddThread( &IdleThread, 255, 0xFFFFFFFF,                         "IDLE____________" );

    // 6. Kill self.
    G8RTOS_KillSelf();
}

/*
 * Thread that receives game state packets from host
 */
void ReceiveDataFromHost()
{
    while(1)
    {

    }
}

/*
 * Thread that sends UDP packets to host
 */
void SendDataToHost()
{
    while(1)
    {

    }
}

/*
 * Thread to read client's joystick
 */
void ReadJoystickClient()
{
    while(1)
    {

    }
}

/*
 * End of game for the client
 */
void EndOfGameClient()
{
    while(1)
    {

    }
}

// ===================== COMMON COMMON COMMON COMMON =========================
/*
 * Thread to draw all the objects in the game
 */
void DrawObjects()
{
    // store all previous locations of the players to see if
    // it needs to be updated.
    PrevPlayer_t prevPlayers[MAX_NUM_OF_PLAYERS];
    for (int i = 0; i < MAX_NUM_OF_PLAYERS; i++)
    {
        prevPlayers[i].Center = -1; // load fake values to determine first run
    }

    // store all previous locations of the balls to see if
    // they need to be updated.
    PrevBall_t prevBalls[MAX_NUM_OF_BALLS];
    for (int i = 0; i < MAX_NUM_OF_BALLS; i++)
    {
        prevBalls[i].CenterX = -1; // load fake values to determine first run
        prevBalls[i].CenterY = -1; // load fake values to determine first run
    }

    while(1)
    {
        // Draw players --------------------
        for (int i = 0; i < playerCount; i++)
        {
            // This player is on its first run. Draw
            // the entire paddle.
            if ( prevPlayers[i].Center == -1 ) DrawPlayer( &gamestate.players[i] );

            // if this player has already been drawn, only
            // update the parts that need to be redrawn.
            else UpdatePlayerOnScreen( &prevPlayers[i], &gamestate.players[i]);

            prevPlayers[i].Center = gamestate.players[i].currentCenter;
        }

        // Redraw balls --------------------
        for (int i = 0; i < ballCount; i++)
        {
            // if already made..
            // UpdateBallOnScreen(previousBall, currentBall, outColor);

            // else if this is the first run...
            // ??
        }

        // Refresh rate --------------------
        sleep(20);
    }
}

/*
 * Thread to update LEDs based on score
 */
void MoveLEDs()
{
    while(1)
    {

    }
}

/*
 * Idle thread to avoid deadlocks and RTOS end
 */
void IdleThread() { while(1); }
