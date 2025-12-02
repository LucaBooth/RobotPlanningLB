#include <stdio.h>
#include <stdlib.h>
#include "rs232.h"
#include "serial.h"

#define bdrate 115200

// fontData 
typedef struct {
    int asciiCode;
    int strokeCount;
    float moves[30][3]; //maximum 30 moves, 3 variables needed
} fontData;

//functions to be called
void loadFont(const char* fileName, fontData fontLibrary[128]);
double ScalingFactor(void);
void processText(double scaleFactor, const char*textFile, fontData fontLibrary[128]);
float wordWidth(const char* c, const fontData fontLibrary[128], double scaleFactor);
void initRobot(int* lastPenState);
void penState(int* lastPenState, int newPenState);
void returnHome(int* lasPenState);
void move(float x, float y, int penState);
void drawCharacter(char c, const fontData fontLibrary[128], double scale, double* xOffset, double* yOffset, int* lastPenState);
void SendCommands (char *buffer );

int main()
{

    int lastPenState = -1; // 1 is pen up, 0 is pen down, -1 so that system has to change value
    double scaleFactor = 1.0; 
    char buffer[100];
    fontData fontLibrary[128];

    // port check
    if ( CanRS232PortBeOpened() == -1 )
    {
        printf ("\nUnable to open the COM port (specified in serial.h) ");
        exit (0);
    }

    // Time to wake up the robot
    printf ("\nAbout to wake up the robot\n");
    sprintf (buffer, "\n");
    PrintBuffer (&buffer[0]);
    Sleep(100);
    WaitForDollar();
    printf ("\nThe robot is now ready to draw\n");

    //flowchart logic start
    initRobot(&lastPenState);
    printf("Loading font data now:\n");
    loadFont("SingleStrokeFont.txt", fontLibrary);
    scaleFactor = ScalingFactor();
    processText(scaleFactor, "text.txt", fontLibrary);    


    // Before we exit the program we need to close the COM port
    CloseRS232Port();
    printf("Com port now closed\n");

    return (0);
}

void SendCommands(char *buffer )
{
    // printf ("Buffer to send: %s", buffer); // For diagnostic purposes only, normally comment out
    PrintBuffer (&buffer[0]);
    WaitForReply();
    //Sleep(100); // Can omit this when using the writing robot but has minimal effect
    // getch(); // Omit this once basic testing with emulator has taken place
}

//robot initialization
void initRobot(int* lastPenState)
 {
    char buffer[100];
    sprintf(buffer, "F1000\n"); SendCommands(buffer); // feed rate
    sprintf(buffer, "M3\n");    SendCommands(buffer); // spindle on
    sprintf(buffer, "S0\n");    SendCommands(buffer); // pen start up
    *lastPenState = 0; // states -> up
}

// load font data
void loadFont(const char* fileName, fontData fontLibrary[128]) {
    FILE *file = fopen(fileName, "r");
    if (file == NULL) {
        printf("Error: Font file not found\n");
        exit(1);
    }

    int marker, ascii, numLines;
    
    // line by line using ascii code 
    while (fscanf(file, "%d %d %d", &marker, &ascii, &numLines) != EOF) {
        if (marker == 999) {
            fontLibrary[ascii].asciiCode = ascii;
            fontLibrary[ascii].strokeCount = numLines;

            // Read the strokes for this character
            for (int i = 0; i < numLines; i++) {
                fscanf(file, "%f %f %f", 
                    &fontLibrary[ascii].moves[i][0],  // x 
                    &fontLibrary[ascii].moves[i][1],  // y
                    &fontLibrary[ascii].moves[i][2]); // pen state
            }
        }
    }
    fclose(file);
    printf("Font loaded successfully.\n");
}

// scaling factor from input from user
double ScalingFactor(void) {
    float height;
    int valid = 0;
    
    do {
        printf("Enter font height (4-10 mm): ");
        scanf(" %f", &height);
        if (height >= 4.0 && height <= 10.0) {
            valid = 1;
        } else {
            printf("Invalid input. Must be 4-10mm.\n");
        }
    } while (valid == 0);
    
    return (double)(height / 18.0);
}

// pen state checker
void penState(int* lastPenState, int newPenState) {
    char buffer[50];
    if (newPenState != *lastPenState) {
        if (newPenState == 1) sprintf(buffer, "S1000\n");
        else sprintf(buffer, "S0\n");
        SendCommands(buffer);
        *lastPenState = newPenState;
    }
}

// move function with gcode commands
void move(float x, float y, int penState) {
    char buffer[100];
    if (penState == 1) sprintf(buffer, "G1 X%.2f Y%.2f\n", x, y);
    else sprintf(buffer, "G0 X%.2f Y%.2f\n", x, y);
    SendCommands(buffer);
}

// returnHome function to go back to origin and reset state
void returnHome(int* lastPenState) {
    penState(lastPenState, 0); // Lift pen
    move(0.0, 0.0, 0);         // Go to origin 0,0
}

// process text 
void processText(double scaleFactor, const char* textFile, fontData fontLibrary[128]) {

    FILE *file = fopen(textFile, "r");
    if (file == NULL) {
        printf("Error: Text file not found\n");
        exit(1);
    }
    double posX =0;
    double posY =0;
    int lastPenState = -1; // start at -1 so pen state function always works
    char wordBuffer[100];
    int bufferTracker = 0; // track position in buffer
    int c; //variable to hold character currently being read
    initRobot(&lastPenState);
    while (1) {
        c=fgetc(file);
        if (c ==' '|| c =='\n'|| c =='\r'|| c ==EOF){
            if (bufferTracker >0){
                wordBuffer[bufferTracker]  = '\0'; // terminate 
                float totalWordWidth = wordWidth(wordBuffer, fontLibrary, scaleFactor); 
                if (posX + totalWordWidth >100){
                    posX =0; // reset to start of line
                    posY -= 5;  // move down a line
                }
            for (int i=0; i<bufferTracker; i++){
            drawCharacter(wordBuffer[i], fontLibrary, scaleFactor, &posX, &posY, &lastPenState);
         }
            bufferTracker =0; 
            if (c==EOF){ // end of file
                break;
            }
            if (c == ' '){
                drawCharacter(' ', fontLibrary, scaleFactor, &posX, &posY, &lastPenState);
            }
            else if (c =='\n'){
            posX =0; // new line and and posX reset
            posY -=5;
            }
            else if (c =='\r'){
            posX =0;
            posY =0; // return home
            }
            else{

             wordBuffer[bufferTracker]= (char)c; // add char to buffer
        bufferTracker++;
            }
    }
}

fclose(file);
        
}
}


// word width
float wordWidth(const char* c, const fontData fontLibrary[128], double scaleFactor)
 { 
    float totalwordWidth =0;
    int i=0; // letter tracker  for loop
    
    while (c[i] != '\0'){
        int ascii = (int)c[i]; //conversion from number to ascii

        if (ascii >= 0 && ascii <128){ //check character is in ascii library
            int lastMove = fontLibrary[ascii].strokeCount -1; // final index is count -1 as array starts at 0
            if (lastMove >=0) { 
                float charWidth = fontLibrary[ascii].moves[lastMove][0]; //x coord of last point shows letter width
                totalwordWidth += charWidth * scaleFactor; // scale it and add to width total
            }

            }
        i++; //next letter
        }
    return totalwordWidth;

    }
//drawCharacter
void drawCharacter(char c, const fontData fontLibrary[128], double scale, double* xOffset, double* yOffset, int* lastPenState) {
    int ascii = (int)c;
    int count = fontLibrary[ascii].strokeCount;

    // draw each stroke
    for (int i = 0; i < count; i++) {
        float localX = fontLibrary[ascii].moves[i][0];
        float localY = fontLibrary[ascii].moves[i][1];
        int pState = (int)fontLibrary[ascii].moves[i][2];
        //calculate postions with scaling and offsets
        double finalX = (localX * scale) + *xOffset;
        double finalY = (localY * scale) + *yOffset;

        penState(lastPenState, pState); 
        move(finalX, finalY, *lastPenState); 
    }

    // update xOff for next character
    int lastIndex = count - 1;
    if (lastIndex >= 0) {
        float charEndWidth = fontLibrary[ascii].moves[lastIndex][0];
        *xOffset += (charEndWidth * scale);
    }
}

