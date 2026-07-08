const char MIN_CHAR = 0x00;
const char MAX_CHAR = 0x0F;
const char SPLIT_AT = 0x07;

unsigned char index = 0x00;
unsigned char val = 0x00;

bool logging = false;

enum Waveform {
  triangleWave,
  squareWave,
  sineWave
};

bool rising = true;

//Waveform waveform = triangleWave;
//Waveform waveform = squareWave;
Waveform waveform = sineWave;

bool sineRising = true;
char sineTable[256] = {8,8,8,8,8,8,9,9,9,9,9,10,10,10,10,10,10,11,11,11,11,11,11,12,12,12,12,12,12,12,13,13,13,13,13,13,13,13,14,14,14,14,14,14,14,14,14,14,14,14,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,14,14,14,14,14,14,14,14,14,14,14,14,13,13,13,13,13,13,13,13,12,12,12,12,12,12,12,11,11,11,11,11,11,10,10,10,10,10,10,9,9,9,9,9,8,8,8,8,8,8,7,7,7,7,7,6,6,6,6,6,5,5,5,5,5,5,4,4,4,4,4,4,3,3,3,3,3,3,3,2,2,2,2,2,2,2,2,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,2,2,2,2,2,2,2,2,3,3,3,3,3,3,3,4,4,4,4,4,4,5,5,5,5,5,5,6,6,6,6,6,7,7,7,7,7};

void setup() {
  Serial.begin(9600);
  Serial.println("BEGIN SETUP...");
  PORTD.DIR = 0x0F;  //Set DA0-3 as output
  Serial.println("END SETUP...");
}

void loop() {
  if(logging) {
    Serial.print(index, DEC);
    Serial.print(": ");
    Serial.println(val, DEC);
  }
  PORTD.OUT = val;
  switch (waveform) {
    case squareWave: {
        updateSquare();
        break;
      }

    case sineWave: {
        updateSine();
        break;
      }

    default: {
        updateTriangle();
        break;
      }
  }
}

void updateTriangle() {
  val++;
}

void updateSquare() {
  if(index < 0x7F) {
    val = MIN_CHAR;
  } else {
    val = MAX_CHAR;
  }
  index++;
}

void updateSine() {
  val = sineTable[index++];
}


//void generateSineTable() {
//  unsigned char tableIndex = 0;
//  Serial.print("char sineTable[255] = {");
//  
//  const unsigned char STOP_VAL = 255;
//  for (unsigned char i = 0x00; i < STOP_VAL; i++) {
//    float v = (0x07 * sin((M_PI / 180) * 1.4 * i) + 0x07);
//    sineTable[tableIndex++] = (char)v;
//
//    Serial.print((int)v, DEC);
//    Serial.println(", ");
//  }
//  Serial.println("};");
//}

