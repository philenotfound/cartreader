//******************************************
// SEGA MEGA DRIVE MODULE
//******************************************
// Writes to Sega CD Backup RAM Cart require an extra wire from MRES (B02) to VRES (B27)

/******************************************
   Variables
 *****************************************/
unsigned long sramEnd;
byte eepbit[8];
int eepSize;
byte eeptemp;
word addrhi;
word addrlo;
word chksum;

//***********************************************
// EEPROM SAVE TYPES
// 1 = Acclaim Type 1    [24C02]
// 2 = Acclaim Type 2    [24C02/24C16/24C65]
// 3 = Capcom/SEGA       [24C01]
// 4 = EA                [24C01]
// 5 = Codemasters       [24C08/24C16/24C65]
//***********************************************
byte eepType;

//*********************************************************
// SERIAL EEPROM LOOKUP TABLE
// Format = {chksum, eepType | eepSize}
// chksum is located in ROM at 0x18E (0xC7)
// eepType and eepSize are combined to conserve memory
//*********************************************************
static const word PROGMEM eepid [] = {
  // ACCLAIM TYPE 1
  0x5B9F, 0x101,  // NBA Jam (J)
  0x694F, 0x101,  // NBA Jam (UE) (Rev 0)
  0xBFA9, 0x101,  // NBA Jam (UE) (Rev 1)
  // ACCLAIM TYPE 2
  0x16B2, 0x102,  // Blockbuster World Videogame Championship II (U)   [NO HEADER SAVE DATA]
  0xCC3F, 0x102,  // NBA Jam Tournament Edition (W) (Rev 0)            [NO HEADER SAVE DATA]
  0x8AE1, 0x102,  // NBA Jam Tournament Edition (W) (Rev 1)            [NO HEADER SAVE DATA]
  0xDB97, 0x102,  // NBA Jam Tournament Edition 32X (W)
  0x7651, 0x102,  // NFL Quarterback Club (W)
  0xDFE4, 0x102,  // NFL Quarterback Club 32X (W)
  0x3DE6, 0x802,  // NFL Quarterback Club '96 (UE)
  0xCB78, 0x2002, // Frank Thomas Big Hurt Baseball (UE)
  0x6DD9, 0x2002, // College Slam (U)
  // CAPCOM
  0xAD23, 0x83,   // Mega Man:  The Wily Wars (E)
  0xEA80, 0x83,   // Rockman Megaworld (J)
  // SEGA
  0x760F, 0x83,   // Evander "Real Deal" Holyfield Boxing (JU)
  0x95E7, 0x83,   // Greatest Heavyweights of the Ring (E)
  0x0000, 0x83,   // Greatest Heavyweights of the Ring (J)             [BLANK CHECKSUM 0000]
  0x7270, 0x83,   // Greatest Heavyweights of the Ring (U)
  0xBACC, 0x83,   // Honoo no Toukyuuji Dodge Danpei (J)
  0xB939, 0x83,   // MLBPA Sports Talk Baseball (U)                    [BAD HEADER SAVE DATA]
  0x487C, 0x83,   // Ninja Burai Densetsu (J)
  0x740D, 0x83,   // Wonder Boy in Monster World (B)
  0x0278, 0x83,   // Wonder Boy in Monster World (J)
  0x9D79, 0x83,   // Wonder Boy in Monster World (UE)
  // EA
  0x8512, 0x84,   // Bill Walsh College Football (UE)                  [BAD HEADER SAVE DATA]
  0xA107, 0x84,   // John Madden Football '93 (UE)                     [NO HEADER SAVE DATA]
  0x5807, 0x84,   // John Madden Football '93 Championship Edition (U) [NO HEADER SAVE DATA]
  0x2799, 0x84,   // NHLPA Hockey '93 (UE) (Rev 0)                     [NO HEADER SAVE DATA]
  0xFA57, 0x84,   // NHLPA Hockey '93 (UE) (Rev 1)                     [NO HEADER SAVE DATA]
  0x8B9F, 0x84,   // Rings of Power (UE)                               [NO HEADER SAVE DATA]
  // CODEMASTERS
  0x7E65, 0x405,  // Brian Lara Cricket (E)                            [NO HEADER SAVE DATA]
  0x9A5C, 0x2005, // Brian Lara Cricket 96 (E) (Rev 1.0)               [NO HEADER SAVE DATA]
  0xC4EE, 0x2005, // Brian Lara Cricket 96 (E) (Rev 1.1)               [NO HEADER SAVE DATA]
  0x7E50, 0x805,  // Micro Machines 2 (E) (J-Cart)                     [NO HEADER SAVE DATA]
  0x165E, 0x805,  // Micro Machines '96 (E) (J-Cart) (Rev 1.0/1.1)     [NO HEADER SAVE DATA]
  0x168B, 0x405,  // Micro Machines Military (E) (J-Cart)              [NO HEADER SAVE DATA]
  0x12C1, 0x2005, // Shane Warne Cricket (E)                           [NO HEADER SAVE DATA]
};

byte eepcount = (sizeof(eepid) / sizeof(eepid[0])) / 2;
int index;
word eepdata;

// CD BACKUP RAM
unsigned long bramSize = 0;

// REALTEC MAPPER
boolean realtec = 0;
/******************************************
   Menu
 *****************************************/
// MD menu items
static const char MDMenuItem1[] PROGMEM = "Game Cartridge";
static const char MDMenuItem2[] PROGMEM = "SegaCD RamCart";
static const char MDMenuItem3[] PROGMEM = "Flash Repro";
static const char MDMenuItem4[] PROGMEM = "Reset";
static const char* const menuOptionsMD[] PROGMEM = {MDMenuItem1, MDMenuItem2, MDMenuItem3, MDMenuItem4};

// Cart menu items
static const char MDCartMenuItem1[] PROGMEM = "Read Rom";
static const char MDCartMenuItem2[] PROGMEM = "Read Sram";
static const char MDCartMenuItem3[] PROGMEM = "Write Sram";
static const char MDCartMenuItem4[] PROGMEM = "Read EEPROM";
static const char MDCartMenuItem5[] PROGMEM = "Write EEPROM";
static const char MDCartMenuItem6[] PROGMEM = "Reset";
static const char* const menuOptionsMDCart[] PROGMEM = {MDCartMenuItem1, MDCartMenuItem2, MDCartMenuItem3, MDCartMenuItem4, MDCartMenuItem5, MDCartMenuItem6};

// Sega CD Ram Backup Cartridge menu items
static const char SCDMenuItem1[] PROGMEM = "Read Backup RAM";
static const char SCDMenuItem2[] PROGMEM = "Write Backup RAM";
static const char SCDMenuItem3[] PROGMEM = "Reset";
static const char* const menuOptionsSCD[] PROGMEM = {SCDMenuItem1, SCDMenuItem2, SCDMenuItem3};

// Sega start menu
void mdMenu() {
  // create menu with title and 4 options to choose from
  unsigned char mdDev;
  // Copy menuOptions out of progmem
  convertPgm(menuOptionsMD, 4);
  mdDev = question_box(F("Select MD device"), menuOptions, 4, 0);

  // wait for user choice to come back from the question box menu
  switch (mdDev)
  {
    case 0:
      display_Clear();
      display_Update();
      setup_MD();
      mode = mode_MD_Cart;
      break;

    case 1:
      display_Clear();
      display_Update();
      setup_MD();
      mode =  mode_SEGA_CD;
      break;

    case 2:
      display_Clear();
      display_Update();
      setup_MD();
      mode =  mode_MD_Cart;
      // Change working dir to root
      filePath[0] = '\0';
      sd.chdir("/");
      fileBrowser(F("Select file"));
      display_Clear();
      // Setting CS(PH3) LOW
      PORTH &= ~(1 << 3);

      // ID flash
      resetFlash_MD();
      idFlash_MD();
      resetFlash_MD();
      print_Msg(F("Flash ID: "));
      println_Msg(flashid);
      if (strcmp(flashid, "C2F1") == 0) {
        println_Msg(F("MX29F1610 detected"));
        flashSize = 2097152;
      }
      else {
        print_Error(F("Error: Unknown flashrom"), true);
      }
      display_Update();

      eraseFlash_MD();
      resetFlash_MD();
      blankcheck_MD();
      write29F1610_MD();
      resetFlash_MD();
      delay(1000);
      resetFlash_MD();
      delay(1000);
      verifyFlash_MD();
      // Set CS(PH3) HIGH
      PORTH |= (1 << 3);
      println_Msg(F(""));
      println_Msg(F("Press Button..."));
      display_Update();
      wait();
      break;

    case 3:
      resetArduino();
      break;
  }
}

void mdCartMenu() {
  // create menu with title and 6 options to choose from
  unsigned char mainMenu;
  // Copy menuOptions out of progmem
  convertPgm(menuOptionsMDCart, 6);
  mainMenu = question_box(F("MEGA DRIVE Reader"), menuOptions, 6, 0);

  // wait for user choice to come back from the question box menu
  switch (mainMenu)
  {
    case 0:
      display_Clear();

      // common ROM read fail state: no cart inserted - tends to report impossibly large cartSize
      // largest known game so far is supposedly "Paprium" at 10MB, so cap sanity check at 16MB
      if (cartSize != 0 && cartSize <= 16777216) {
        // Change working dir to root
        sd.chdir("/");
        if (realtec)
          readRealtec_MD();
        else
          readROM_MD();
        //compare_checksum_MD();
      }
      else {
        print_Error(F("Cart has no ROM"), false);
      }
      break;

    case 1:
      display_Clear();
      // Does cartridge have SRAM
      if ((saveType == 1) || (saveType == 2) || (saveType == 3)) {
        // Change working dir to root
        sd.chdir("/");
        println_Msg(F("Reading Sram..."));
        display_Update();
        enableSram_MD(1);
        readSram_MD();
        enableSram_MD(0);
      }
      else {
        print_Error(F("Cart has no Sram"), false);
      }
      break;

    case 2:
      display_Clear();
      // Does cartridge have SRAM
      if ((saveType == 1) || (saveType == 2) || (saveType == 3)) {
        // Change working dir to root
        sd.chdir("/");
        // Launch file browser
        fileBrowser(F("Select srm file"));
        display_Clear();
        enableSram_MD(1);
        writeSram_MD();
        writeErrors = verifySram_MD();
        enableSram_MD(0);
        if (writeErrors == 0) {
          println_Msg(F("Sram verified OK"));
          display_Update();
        }
        else {
          print_Msg(F("Error: "));
          print_Msg(writeErrors);
          println_Msg(F(" bytes "));
          print_Error(F("did not verify."), false);
        }
      }
      else {
        print_Error(F("Cart has no Sram"), false);
      }
      break;

    case 3:
      display_Clear();
      if (saveType == 4)
        readEEP_MD();
      else {
        print_Error(F("Cart has no EEPROM"), false);
      }
      break;

    case 4:
      display_Clear();
      if (saveType == 4) {
        // Launch file browser
        fileBrowser(F("Select eep file"));
        display_Clear();
        writeEEP_MD();
      }
      else {
        print_Error(F("Cart has no EEPROM"), false);
      }
      break;

    case 5:
      // Reset
      resetArduino();
      break;
  }
  println_Msg(F(""));
  println_Msg(F("Press Button..."));
  display_Update();
  wait();
}

void segaCDMenu() {
  // create menu with title and 3 options to choose from
  unsigned char scdMenu;
  // Copy menuOptions out of progmem
  convertPgm(menuOptionsSCD, 3);
  scdMenu = question_box(F("SEGA CD RAM"), menuOptions, 3, 0);

  // wait for user choice to come back from the question box menu
  switch (scdMenu)
  {
    case 0:
      display_Clear();
      if (bramSize > 0)
        readBram_MD();
      else {
        print_Error(F("Not CD Backup RAM Cart"), false);
      }
      break;

    case 1:
      display_Clear();
      if (bramSize > 0) {
        // Launch file browser
        fileBrowser(F("Select brm file"));
        display_Clear();
        writeBram_MD();
      }
      else {
        print_Error(F("Not CD Backup RAM Cart"), false);
      }
      break;

    case 2:
      // Reset
      asm volatile ("  jmp 0");
      break;
  }
  println_Msg(F(""));
  println_Msg(F("Press Button..."));
  display_Update();
  wait();
}

/******************************************
   Setup
 *****************************************/
void setup_MD() {
  // Set Address Pins to Output
  //A0-A7
  DDRF = 0xFF;
  //A8-A15
  DDRK = 0xFF;
  //A16-A23
  DDRL = 0xFF;

  // Set Control Pins to Output RST(PH0) CLK(PH1) CS(PH3) WRH(PH4) WRL(PH5) OE(PH6)
  DDRH |=  (1 << 0) | (1 << 1) | (1 << 3) | (1 << 4) | (1 << 5) | (1 << 6);

  // Set TIME(PJ0) to Output
  DDRJ |=  (1 << 0);

  // Set Data Pins (D0-D15) to Input
  DDRC = 0x00;
  DDRA = 0x00;

  // Setting RST(PH0) CLK(PH1) CS(PH3) WRH(PH4) WRL(PH5) OE(PH6) HIGH
  PORTH |= (1 << 0) | (1 << 1) | (1 << 3) | (1 << 4) | (1 << 5) | (1 << 6);

  // Setting TIME(PJ0) HIGH
  PORTJ |= (1 << 0);

  delay(200);

  // Print all the info
  getCartInfo_MD();
}

/******************************************
   I/O Functions
 *****************************************/

/******************************************
  Low level functions
*****************************************/
void writeWord_MD(unsigned long myAddress, word myData) {
  PORTF = myAddress & 0xFF;
  PORTK = (myAddress >> 8) & 0xFF;
  PORTL = (myAddress >> 16) & 0xFF;
  PORTC = myData;
  PORTA = (myData >> 8) & 0xFF;

  // Arduino running at 16Mhz -> one nop = 62.5ns
  // Wait till output is stable
  __asm__("nop\n\t""nop\n\t");

  // Switch WR(PH5) to LOW
  PORTH &= ~(1 << 5);
  // Setting CS(PH3) LOW
  PORTH &= ~(1 << 3);

  // Leave WR low for at least 200ns
  __asm__("nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t");

  // Setting CS(PH3) HIGH
  PORTH |= (1 << 3);
  // Switch WR(PH5) to HIGH
  PORTH |= (1 << 5);

  // Leave WR high for at least 50ns
  __asm__("nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t");
}

word readWord_MD(unsigned long myAddress) {
  PORTF = myAddress & 0xFF;
  PORTK = (myAddress >> 8) & 0xFF;
  PORTL = (myAddress >> 16) & 0xFF;

  // Arduino running at 16Mhz -> one nop = 62.5ns
  NOP;

  // Setting CS(PH3) LOW
  PORTH &= ~(1 << 3);
  // Setting OE(PH6) LOW
  PORTH &= ~(1 << 6);

  // most MD ROMs are 200ns, comparable to SNES > use similar access delay of 6 x 62.5 = 375ns
  NOP; NOP; NOP; NOP; NOP; NOP;

  // Read
  word tempWord = ( ( PINA & 0xFF ) << 8 ) | ( PINC & 0xFF );

  // Setting CS(PH3) HIGH
  PORTH |= (1 << 3);
  // Setting OE(PH6) HIGH
  PORTH |= (1 << 6);

  // these 6x nop delays have been here before, unknown what they mean
  NOP; NOP; NOP; NOP; NOP; NOP;

  return tempWord;
}

void writeFlash_MD(unsigned long myAddress, word myData) {
  PORTF = myAddress & 0xFF;
  PORTK = (myAddress >> 8) & 0xFF;
  PORTL = (myAddress >> 16) & 0xFF;
  PORTC = myData;
  PORTA = (myData >> 8) & 0xFF;

  // Arduino running at 16Mhz -> one nop = 62.5ns
  // Wait till output is stable
  __asm__("nop\n\t");

  // Switch WE(PH5) to LOW
  PORTH &= ~(1 << 5);

  // Leave WE low for at least 60ns
  __asm__("nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t");

  // Switch WE(PH5)to HIGH
  PORTH |= (1 << 5);

  // Leave WE high for at least 50ns
  __asm__("nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t");
}

word readFlash_MD(unsigned long myAddress) {
  PORTF = myAddress & 0xFF;
  PORTK = (myAddress >> 8) & 0xFF;
  PORTL = (myAddress >> 16) & 0xFF;

  // Arduino running at 16Mhz -> one nop = 62.5ns
  __asm__("nop\n\t");

  // Setting OE(PH6) LOW
  PORTH &= ~(1 << 6);

  __asm__("nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t");

  // Read
  word tempWord = ( ( PINA & 0xFF ) << 8 ) | ( PINC & 0xFF );

  __asm__("nop\n\t");

  // Setting OE(PH6) HIGH
  PORTH |= (1 << 6);
  __asm__("nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t");

  return tempWord;
}

// Switch data pins to write
void dataOut_MD() {
  DDRC = 0xFF;
  DDRA = 0xFF;
}

// Switch data pins to read
void dataIn_MD() {
  DDRC = 0x00;
  DDRA = 0x00;
}

/******************************************
  MEGA DRIVE functions
*****************************************/
void getCartInfo_MD() {
  // Set control
  dataIn_MD();

  cartSize = ((long(readWord_MD(0xD2)) << 16) | readWord_MD(0xD3)) + 1;

  // Cart Checksum
  chksum = readWord_MD(0xC7);

  // Super Street Fighter 2 Check
  if (cartSize == 0x400000) {
    switch (chksum) {
      case 0xCE25: // Super Street Fighter 2 (J) 40Mbit
      case 0xE41D: // Super Street Fighter 2 (E) 40Mbit
      case 0xE017: // Super Street Fighter 2 (U) 40Mbit
        cartSize = 0x500000;
        break;
    }
  }

  // Serial EEPROM Check
  for (int i = 0; i < eepcount; i++) {
    index = i * 2;
    word eepcheck = pgm_read_word(eepid + index);
    if (eepcheck == chksum) {
      eepdata = pgm_read_word(eepid + index + 1);
      eepType = eepdata & 0x7;
      eepSize = eepdata & 0xFFF8;
      saveType = 4; // SERIAL EEPROM
      break;
    }
  }

  // Greatest Heavyweights of the Ring (J) has blank chksum 0x0000
  // Other non-save carts might have the same blank chksum
  // Check header for Serial EEPROM Data
  if (chksum == 0x0000) {
    if (readWord_MD(0xD9) != 0xE840) { // NOT SERIAL EEPROM
      eepType = 0;
      eepSize = 0;
      saveType = 0;
    }
  }

  // Codemasters EEPROM Check
  // Codemasters used the same incorrect header across multiple carts
  // Carts with checksum 0x165E or 0x168B could be EEPROM or non-EEPROM
  // Check the first DWORD in ROM (0x444E4C44) to identify the EEPROM carts
  if ((chksum == 0x165E) || (chksum == 0x168B)) {
    if (readWord_MD(0x00) != 0x444E) { // NOT SERIAL EEPROM
      eepType = 0;
      eepSize = 0;
      saveType = 0;
    }
  }

  // CD Backup RAM Cart Check
  // 4 = 128KB (2045 Blocks) Sega CD Backup RAM Cart
  // 6 = 512KB (8189 Blocks) Ultra CD Backup RAM Cart (Aftermarket)
  word bramCheck = readWord_MD(0x00);
  if (((bramCheck == 0x0004) && (chksum == 0x0004)) || ((bramCheck == 0x0006) && (chksum == 0x0006)))
    bramSize = pow(2, bramCheck) * 0x2000;
  if (saveType != 4) { // NOT SERIAL EEPROM
    // Check if cart has sram
    saveType = 0;
    sramSize = 0;

    // FIXED CODE FOR SRAM/FRAM/PARALLEL EEPROM
    // 0x5241F820 SRAM (ODD BYTES/EVEN BYTES)
    // 0x5241F840 PARALLEL EEPROM - READ AS SRAM
    // 0x5241E020 SRAM (BOTH BYTES)
    if (readWord_MD(0xD8) == 0x5241) {
      word sramType = readWord_MD(0xD9);
      if ((sramType == 0xF820) || (sramType == 0xF840)) { // SRAM/FRAM ODD/EVEN BYTES
        // Get sram start and end
        sramBase = ((long(readWord_MD(0xDA)) << 16) | readWord_MD(0xDB));
        sramEnd = ((long(readWord_MD(0xDC)) << 16) | readWord_MD(0xDD));

        // Check alignment of sram
        if ((sramBase == 0x200001) || (sramBase == 0x300001)) { // ADDED 0x300001 FOR HARDBALL '95 (U)
          // low byte
          saveType = 1; // ODD
          sramSize = (sramEnd - sramBase + 2) / 2;
          // Right shift sram base address so [A21] is set to high 0x200000 = 0b001[0]00000000000000000000
          sramBase = sramBase >> 1;
        }
        else if (sramBase == 0x200000) {
          // high byte
          saveType = 2; // EVEN
          sramSize = (sramEnd - sramBase + 1) / 2;
          // Right shift sram base address so [A21] is set to high 0x200000 = 0b001[0]00000000000000000000
          sramBase = sramBase / 2;
        }
        else
          print_Error(F("Unknown Sram Base"), true);
      }
      else if (sramType == 0xE020) { // SRAM BOTH BYTES
        // Get sram start and end
        sramBase = ((long(readWord_MD(0xDA)) << 16) | readWord_MD(0xDB));
        sramEnd = ((long(readWord_MD(0xDC)) << 16) | readWord_MD(0xDD));

        if (sramBase == 0x200001) {
          saveType = 3; // BOTH
          sramSize = sramEnd - sramBase + 2;
          sramBase = sramBase >> 1;
        }
        else if (sramBase == 0x200000) {
          saveType = 3; // BOTH
          sramSize = sramEnd - sramBase + 1;
          sramBase = sramBase >> 1;
        }
        else
          print_Error(F("Unknown Sram Base"), true);
      }
    }
    else {
      // SRAM CARTS WITH BAD/MISSING HEADER SAVE DATA
      switch (chksum) {
        case 0xC2DB:  // Winter Challenge (UE)
          saveType = 1; // ODD
          sramBase = 0x200001;
          sramEnd = 0x200FFF;
          break;

        case 0xD7B6: // Buck Rogers: Countdown to Doomsday (UE)
        case 0xFE3E: // NBA Live '98 (U)
        case 0xFDAD: // NFL '94 starring Joe Montana (U)
        case 0x632E: // PGA Tour Golf (UE) (Rev 1)
        case 0xD2BA: // PGA Tour Golf (UE) (Rev 2)
        case 0x44FE: // Super Hydlide (J)
          saveType = 1; // ODD
          sramBase = 0x200001;
          sramEnd = 0x203FFF;
          break;

        case 0xDB5E: // Might & Magic: Gates to Another World (UE) (Rev 1)
        case 0x3428: // Starflight (UE) (Rev 0)
        case 0x43EE: // Starflight (UE) (Rev 1)
          saveType = 3; // BOTH
          sramBase = 0x200001;
          sramEnd = 0x207FFF;
          break;

        case 0xBF72: // College Football USA '96 (U)
        case 0x72EF: // FIFA Soccer '97 (UE)
        case 0xD723: // Hardball III (U)
        case 0x06C1: // Madden NFL '98 (U)
        case 0xDB17: // NHL '96 (UE)
        case 0x5B3A: // NHL '98 (U)
        case 0x2CF2: // NFL Sports Talk Football '93 starring Joe Montana (UE)
        case 0xE9B1: // Summer Challenge (U)
        case 0xEEE8: // Test Drive II: The Duel (U)
          saveType = 1; // ODD
          sramBase = 0x200001;
          sramEnd = 0x20FFFF;
          break;
      }
      if (saveType == 1) {
        sramSize = (sramEnd - sramBase + 2) / 2;
        sramBase = sramBase >> 1;
      }
      else if (saveType == 3) {
        sramSize = sramEnd - sramBase + 2;
        sramBase = sramBase >> 1;
      }
    }
  }

  // Get name
  for (byte c = 0; c < 48; c += 2) {
    // split word
    word myWord = readWord_MD((0x150 + c) / 2);
    byte loByte = myWord & 0xFF;
    byte hiByte = myWord >> 8;

    // write to buffer
    sdBuffer[c] = hiByte;
    sdBuffer[c + 1] = loByte;
  }
  byte myLength = 0;
  for (unsigned int i = 0; i < 48; i++) {
    if (((char(sdBuffer[i]) >= 48 && char(sdBuffer[i]) <= 57) || (char(sdBuffer[i]) >= 65 && char(sdBuffer[i]) <= 122)) && myLength < 15) {
      romName[myLength] = char(sdBuffer[i]);
      myLength++;
    }
  }

  // Realtec Mapper Check
  word realtecCheck1 = readWord_MD(0x3F080); // 0x7E100 "SEGA" (BootROM starts at 0x7E000)
  word realtecCheck2 = readWord_MD(0x3F081);
  if ((realtecCheck1 == 0x5345) && (realtecCheck2 == 0x4741)) {
    realtec = 1;
    strcpy(romName, "Realtec");
    cartSize = 0x80000;
  }

  display_Clear();
  println_Msg(F("Cart Info"));
  println_Msg(F(" "));
  print_Msg(F("Name: "));
  println_Msg(romName);
  print_Msg(F("Size: "));
  print_Msg(cartSize * 8 / 1024 / 1024 );
  println_Msg(F(" MBit"));
  print_Msg(F("ChkS: "));
  print_Msg((chksum >> 8), HEX);
  print_Msg((chksum & 0x00ff), HEX);
  println_Msg(F(""));
  if (saveType == 4) {
    print_Msg(F("Serial EEPROM: "));
    print_Msg(eepSize * 8 / 1024);
    println_Msg(F(" KBit"));
  }
  else {
    print_Msg(F("Sram: "));
    if (sramSize > 0) {
      print_Msg(sramSize * 8 / 1024);
      println_Msg(F(" KBit"));
    }
    else
      println_Msg(F("None"));
  }
  println_Msg(F(" "));

  // Wait for user input
#ifdef enable_OLED
  println_Msg(F("Press Button..."));
  display_Update();
  wait();
#endif
}

void writeSSF2Map(unsigned long myAddress, word myData) {
  dataOut_MD();

  // Set TIME(PJ0) HIGH
  PORTJ |= (1 << 0);

  // 0x50987E * 2 = 0xA130FD  Bank 6 (0x300000-0x37FFFF)
  // 0x50987F * 2 = 0xA130FF  Bank 7 (0x380000-0x3FFFFF)
  PORTL = (myAddress >> 16) & 0xFF;
  PORTK = (myAddress >> 8) & 0xFF;
  PORTF = myAddress & 0xFF;
  PORTC = myData;
  PORTA = (myData >> 8) & 0xFF;

  // Arduino running at 16Mhz -> one nop = 62.5ns
  // Wait till output is stable
  __asm__("nop\n\t""nop\n\t");

  // Strobe TIME(PJ0) LOW to latch the data
  PORTJ &= ~(1 << 0);
  // Switch WR(PH5) to LOW
  PORTH &= ~(1 << 5);

  // Leave WR low for at least 200ns
  __asm__("nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t");

  // Switch WR(PH5) to HIGH
  PORTH |= (1 << 5);
  // Set TIME(PJ0) HIGH
  PORTJ |= (1 << 0);

  dataIn_MD();
}

// Read rom and save to the SD card
void readROM_MD() {
  // Checksum
  uint16_t calcCKS = 0;

  // Set control
  dataIn_MD();

  // Get name, add extension and convert to char array for sd lib
  strcpy(fileName, romName);
  strcat(fileName, ".MD");

  // create a new folder
  EEPROM_readAnything(0, foldern);
  sprintf(folder, "MD/ROM/%s/%d", romName, foldern);
  sd.mkdir(folder, true);
  sd.chdir(folder);

  display_Clear();
  print_Msg(F("Saving to "));
  print_Msg(folder);
  println_Msg(F("/..."));
  display_Update();

  // write new folder number back to eeprom
  foldern = foldern + 1;
  EEPROM_writeAnything(0, foldern);

  // Open file on sd card
  if (!myFile.open(fileName, O_RDWR | O_CREAT)) {
    print_Error(F("SD Error"), true);
  }

  byte buffer[1024] = { 0 };

  // get current time
  unsigned long startTime = millis();

  // Prepare SSF2 Banks
  if (cartSize > 0x400000) {
    writeSSF2Map(0x50987E, 6); // 0xA130FD
    writeSSF2Map(0x50987F, 7); // 0xA130FF
  }
  byte offsetSSF2Bank = 0;
  word d = 0;

  //Initialize progress bar
  uint32_t processedProgressBar = 0;
  uint32_t totalProgressBar = (uint32_t)(cartSize);
  draw_progressbar(0, totalProgressBar);

  for (unsigned long currBuffer = 0; currBuffer < cartSize / 2; currBuffer += 512) {
    // Blink led
    if (currBuffer % 16384 == 0)
      PORTB ^= (1 << 4);

    if (currBuffer == 0x200000) {
      writeSSF2Map(0x50987E, 8); // 0xA130FD
      offsetSSF2Bank = 1;
    }
    else if (currBuffer == 0x240000) {
      writeSSF2Map(0x50987F, 9); // 0xA130FF
      offsetSSF2Bank = 1;
    }

    d = 0;

    for (int currWord = 0; currWord < 512; currWord++) {
      unsigned long myAddress = currBuffer + currWord - (offsetSSF2Bank * 0x80000);
      PORTF = myAddress & 0xFF;
      PORTK = (myAddress >> 8) & 0xFF;
      PORTL = (myAddress >> 16) & 0xFF;

      // Arduino running at 16Mhz -> one nop = 62.5ns
      NOP;
      // Setting CS(PH3) LOW
      PORTH &= ~(1 << 3);
      // Setting OE(PH6) LOW
      PORTH &= ~(1 << 6);
      // most MD ROMs are 200ns, comparable to SNES > use similar access delay of 6 x 62.5 = 375ns
      NOP; NOP; NOP; NOP; NOP; NOP;

      // Read
      buffer[d]     = PINA;
      buffer[d + 1] = PINC;

      // Setting CS(PH3) HIGH
      PORTH |= (1 << 3);
      // Setting OE(PH6) HIGH
      PORTH |= (1 << 6);

      // Skip first 256 words
      if (((currBuffer == 0) && (currWord >= 256)) || (currBuffer > 0)) {
        calcCKS += ((buffer[d] << 8) | buffer[d + 1]);
      }
      d += 2;
    }
    myFile.write(buffer, 1024);

    // update progress bar
    processedProgressBar += 1024;
    draw_progressbar(processedProgressBar, totalProgressBar);
  }
  // Close the file:
  myFile.close();

  // Reset SSF2 Banks
  if (cartSize > 0x400000) {
    writeSSF2Map(0x50987E, 6); // 0xA130FD
    writeSSF2Map(0x50987F, 7); // 0xA130FF
  }

  // print elapsed time
  print_Msg(F("Time elapsed: "));
  print_Msg((millis() - startTime) / 1000);
  println_Msg(F("s"));
  display_Update();

  // print Checksum
  if (chksum == calcCKS) {
    println_Msg(F("Checksum OK"));
    display_Update();
  }
  else {
    print_Msg(F("Checksum Error: "));
    char calcsumStr[5];
    sprintf(calcsumStr, "%04X", calcCKS);
    println_Msg(calcsumStr);
    print_Error(F(""), false);
    display_Update();
  }
}

/******************************************
  SRAM functions
*****************************************/
// Sonic 3 sram enable
void enableSram_MD(boolean enableSram) {
  dataOut_MD();

  // Set D0 to either 1(enable SRAM) or 0(enable ROM)
  PORTC = enableSram;

  // Strobe TIME(PJ0) LOW to latch the data
  PORTJ &= ~(1 << 0);
  __asm__("nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t");
  // Set TIME(PJ0) HIGH
  PORTJ |= (1 << 0);
  __asm__("nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t");

  dataIn_MD();
}

// Write sram to cartridge
void writeSram_MD() {
  dataOut_MD();

  // Create filepath
  sprintf(filePath, "%s/%s", filePath, fileName);
  println_Msg(F("Writing..."));
  println_Msg(filePath);
  display_Update();

  // Open file on sd card
  if (myFile.open(filePath, O_READ)) {
    // Write to the lower byte
    if (saveType == 1) {
      for (unsigned long currByte = sramBase; currByte < sramBase + sramSize; currByte++) {
        writeWord_MD(currByte, (myFile.read() & 0xFF));
      }
    }
    // Write to the upper byte
    else if (saveType == 2) {
      for (unsigned long currByte = sramBase; currByte < sramBase + sramSize; currByte++) {
        writeWord_MD(currByte, ((myFile.read() << 8 ) & 0xFF));
      }
    }
    else
      print_Error(F("Unknown save type"), false);

    // Close the file:
    myFile.close();
    println_Msg(F("Done"));
    display_Update();
  }
  else {
    print_Error(F("SD Error"), true);
  }
  dataIn_MD();
}

// Read sram and save to the SD card
void readSram_MD() {
  dataIn_MD();

  // Get name, add extension and convert to char array for sd lib
  strcpy(fileName, romName);
  strcat(fileName, ".srm");

  // create a new folder for the save file
  EEPROM_readAnything(0, foldern);
  sprintf(folder, "MD/SAVE/%s/%d", romName, foldern);
  sd.mkdir(folder, true);
  sd.chdir(folder);

  // write new folder number back to eeprom
  foldern = foldern + 1;
  EEPROM_writeAnything(0, foldern);

  // Open file on sd card
  if (!myFile.open(fileName, O_RDWR | O_CREAT)) {
    print_Error(F("SD Error"), true);
  }

  for (unsigned long currBuffer = sramBase; currBuffer < sramBase + sramSize; currBuffer += 256) {
    for (int currWord = 0; currWord < 256; currWord++) {
      word myWord = readWord_MD(currBuffer + currWord);

      if (saveType == 2) {
        // Only use the upper byte
        sdBuffer[currWord] = (( myWord >> 8 ) & 0xFF);
      }
      else if (saveType == 1) {
        // Only use the lower byte
        sdBuffer[currWord] = (myWord & 0xFF);
      }
      else { // saveType == 3 (BOTH)
        sdBuffer[currWord * 2] = (( myWord >> 8 ) & 0xFF);
        sdBuffer[(currWord * 2) + 1] = (myWord & 0xFF);
      }
    }
    if (saveType == 3)
      myFile.write(sdBuffer, 512);
    else
      myFile.write(sdBuffer, 256);
  }
  // Close the file:
  myFile.close();
  print_Msg(F("Saved to "));
  print_Msg(folder);
  println_Msg(F("/"));
  display_Update();
}

unsigned long verifySram_MD() {
  dataIn_MD();
  writeErrors = 0;

  // Open file on sd card
  if (myFile.open(filePath, O_READ)) {
    for (unsigned long currBuffer = sramBase; currBuffer < sramBase + sramSize; currBuffer += 512) {
      for (int currWord = 0; currWord < 512; currWord++) {
        word myWord = readWord_MD(currBuffer + currWord);

        if (saveType == 2) {
          // Only use the upper byte
          sdBuffer[currWord] = (( myWord >> 8 ) & 0xFF);
        }
        else if (saveType == 1) {
          // Only use the lower byte
          sdBuffer[currWord] = (myWord & 0xFF);
        }
      }
      // Check sdBuffer content against file on sd card
      for (int i = 0; i < 512; i++) {
        if (myFile.read() != sdBuffer[i]) {
          writeErrors++;
        }
      }
    }

    // Close the file:
    myFile.close();
  }
  else {
    print_Error(F("SD Error"), true);
  }
  // Return 0 if verified ok, or number of errors
  return writeErrors;
}

//******************************************
// Flashrom Functions
//******************************************
void resetFlash_MD() {
  // Set data pins to output
  dataOut_MD();

  // Reset command sequence
  writeFlash_MD(0x5555, 0xaa);
  writeFlash_MD(0x2aaa, 0x55);
  writeFlash_MD(0x5555, 0xf0);

  // Set data pins to input again
  dataIn_MD();
}

void write29F1610_MD() {
  // Create filepath
  sprintf(filePath, "%s/%s", filePath, fileName);
  print_Msg(F("Flashing file "));
  print_Msg(filePath);
  println_Msg(F("..."));
  display_Update();

  // Open file on sd card
  if (myFile.open(filePath, O_READ)) {
    // Get rom size from file
    fileSize = myFile.fileSize();
    if (fileSize > flashSize) {
      print_Error(F("File size exceeds flash size."), true);
    }
    // Set data pins to output
    dataOut_MD();

    // Fill sdBuffer with 1 page at a time then write it repeat until all bytes are written
    int d = 0;
    for (unsigned long currByte = 0; currByte < fileSize / 2; currByte += 64) {
      myFile.read(sdBuffer, 128);

      // Blink led
      if (currByte % 4096 == 0) {
        PORTB ^= (1 << 4);
      }

      // Write command sequence
      writeFlash_MD(0x5555, 0xaa);
      writeFlash_MD(0x2aaa, 0x55);
      writeFlash_MD(0x5555, 0xa0);

      // Write one full page at a time
      for (byte c = 0; c < 64; c++) {
        word currWord = ( ( sdBuffer[d] & 0xFF ) << 8 ) | ( sdBuffer[d + 1] & 0xFF );
        writeFlash_MD(currByte + c, currWord);
        d += 2;
      }
      d = 0;

      // Check if write is complete
      delayMicroseconds(100);
      busyCheck_MD();
    }

    // Set data pins to input again
    dataIn_MD();

    // Close the file:
    myFile.close();
  }
  else {
    println_Msg(F("Can't open file"));
    display_Update();
  }
}

void idFlash_MD() {
  // Set data pins to output
  dataOut_MD();

  // ID command sequence
  writeFlash_MD(0x5555, 0xaa);
  writeFlash_MD(0x2aaa, 0x55);
  writeFlash_MD(0x5555, 0x90);

  // Set data pins to input again
  dataIn_MD();

  // Read the two id bytes into a string
  sprintf(flashid, "%02X%02X", readFlash_MD(0) & 0xFF, readFlash_MD(1) & 0xFF);
}

byte readStatusReg_MD() {
  // Set data pins to output
  dataOut_MD();

  // Status reg command sequence
  writeFlash_MD(0x5555, 0xaa);
  writeFlash_MD(0x2aaa, 0x55);
  writeFlash_MD(0x5555, 0x70);

  // Set data pins to input again
  dataIn_MD();

  // Read the status register
  byte statusReg = readFlash_MD(0);
  return statusReg;
}

void eraseFlash_MD() {
  // Set data pins to output
  dataOut_MD();

  // Erase command sequence
  writeFlash_MD(0x5555, 0xaa);
  writeFlash_MD(0x2aaa, 0x55);
  writeFlash_MD(0x5555, 0x80);
  writeFlash_MD(0x5555, 0xaa);
  writeFlash_MD(0x2aaa, 0x55);
  writeFlash_MD(0x5555, 0x10);

  // Set data pins to input again
  dataIn_MD();

  busyCheck_MD();
}

void blankcheck_MD() {
  blank = 1;
  for (unsigned long currByte = 0; currByte < flashSize / 2; currByte++) {
    if (readFlash_MD(currByte) != 0xFFFF) {
      currByte = flashSize / 2;
      blank = 0;
    }
    if (currByte % 4096 == 0) {
      PORTB ^= (1 << 4);
    }
  }
  if (!blank) {
    print_Error(F("Error: Not blank"), false);
  }
}

void verifyFlash_MD() {
  // Open file on sd card
  if (myFile.open(filePath, O_READ)) {
    // Get rom size from file
    fileSize = myFile.fileSize();
    if (fileSize > flashSize) {
      print_Error(F("File size exceeds flash size."), true);
    }

    blank = 0;
    word d = 0;
    for (unsigned long currByte = 0; currByte < fileSize / 2; currByte += 256) {
      if (currByte % 4096 == 0) {
        PORTB ^= (1 << 4);
      }
      //fill sdBuffer
      myFile.read(sdBuffer, 512);
      for (int c = 0; c < 256; c++) {
        word currWord = ((sdBuffer[d] << 8) | sdBuffer[d + 1]);

        if (readFlash_MD(currByte + c) != currWord) {
          blank++;
        }
        d += 2;
      }
      d = 0;
    }
    if (blank == 0) {
      println_Msg(F("Flashrom verified OK"));
      display_Update();
    }
    else {
      print_Msg(F("Error: "));
      print_Msg(blank);
      println_Msg(F(" bytes "));
      print_Error(F("did not verify."), false);
    }
    // Close the file:
    myFile.close();
  }
  else {
    println_Msg(F("Can't open file"));
    display_Update();
  }
}

// Delay between write operations based on status register
void busyCheck_MD() {
  // Set data pins to input
  dataIn_MD();

  // Read the status register
  word statusReg = readFlash_MD(0);

  while ((statusReg | 0xFF7F) != 0xFFFF) {
    statusReg = readFlash_MD(0);
  }

  // Set data pins to output
  dataOut_MD();
}

//******************************************
// EEPROM Functions
//******************************************
void EepromInit(byte eepmode) { // Acclaim Type 2
  PORTF = 0x00; // ADDR A0-A7
  PORTK = 0x00; // ADDR A8-A15
  PORTL = 0x10; // ADDR A16-A23
  PORTA = 0x00; // DATA D8-D15
  PORTH |= (1 << 0); // /RES HIGH

  PORTC = eepmode; // EEPROM Switch:  0 = Enable (Read EEPROM), 1 = Disable (Read ROM)
  PORTH &= ~(1 << 3); // CE LOW
  PORTH &= ~(1 << 4) & ~(1 << 5); // /UDSW + /LDSW LOW
  PORTH |= (1 << 6); // OE HIGH
  __asm__("nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t");
  PORTH |= (1 << 4) | (1 << 5); // /UDSW + /LDSW HIGH
  __asm__("nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t");
}

void writeWord_SDA(unsigned long myAddress, word myData) { /* D0 goes to /SDA when only /LWR is asserted */
  PORTF = myAddress & 0xFF;
  PORTK = (myAddress >> 8) & 0xFF;
  PORTL = (myAddress >> 16) & 0xFF;
  PORTC = myData;
  PORTH &= ~(1 << 3); // CE LOW
  PORTH &= ~(1 << 5); // /LDSW LOW
  PORTH |= (1 << 4); // /UDSW HIGH
  PORTH |= (1 << 6); // OE HIGH
  if (eepSize > 0x100)
    __asm__("nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t");
  else
    delayMicroseconds(100);
  PORTH |= (1 << 5); // /LDSW HIGH
  if (eepSize > 0x100)
    __asm__("nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t");
  else
    delayMicroseconds(100);
}

void writeWord_SCL(unsigned long myAddress, word myData) { /* D0 goes to /SCL when only /UWR is asserted */
  PORTF = myAddress & 0xFF;
  PORTK = (myAddress >> 8) & 0xFF;
  PORTL = (myAddress >> 16) & 0xFF;
  PORTC = myData;
  PORTH &= ~(1 << 3); // CE LOW
  PORTH &= ~(1 << 4); // /UDSW LOW
  PORTH |= (1 << 5); // /LDSW HIGH
  PORTH |= (1 << 6); // OE HIGH
  if (eepSize > 0x100)
    __asm__("nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t");
  else
    delayMicroseconds(100);
  PORTH |= (1 << 4); // /UDSW HIGH
  if (eepSize > 0x100)
    __asm__("nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t");
  else
    delayMicroseconds(100);
}

void writeWord_CM(unsigned long myAddress, word myData) { // Codemasters
  PORTF = myAddress & 0xFF;
  PORTK = (myAddress >> 8) & 0xFF;
  PORTL = (myAddress >> 16) & 0xFF;
  PORTC = myData;
  PORTA = (myData >> 8) & 0xFF;

  // Arduino running at 16Mhz -> one nop = 62.5ns
  // Wait till output is stable
  __asm__("nop\n\t""nop\n\t");

  // Switch WR(PH4) to LOW
  PORTH &= ~(1 << 4);
  // Setting CS(PH3) LOW
  PORTH &= ~(1 << 3);
  // Pulse CLK(PH1)
  PORTH ^= (1 << 1);

  // Leave WR low for at least 200ns
  __asm__("nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t");

  // Pulse CLK(PH1)
  PORTH ^= (1 << 1);
  // Setting CS(PH3) HIGH
  PORTH |= (1 << 3);
  // Switch WR(PH4) to HIGH
  PORTH |= (1 << 4);

  // Leave WR high for at least 50ns
  __asm__("nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t");
}

// EEPROM COMMANDS
void EepromStart() {
  if (eepType == 2) { // Acclaim Type 2
    writeWord_SDA(0x100000, 0x00); // sda low
    writeWord_SCL(0x100000, 0x00); // scl low
    writeWord_SDA(0x100000, 0x01); // sda high
    writeWord_SCL(0x100000, 0x01); // scl high
    writeWord_SDA(0x100000, 0x00); // sda low
    writeWord_SCL(0x100000, 0x00); // scl low
  }
  else if (eepType == 4) { // EA
    writeWord_MD(0x100000, 0x00); // sda low, scl low
    writeWord_MD(0x100000, 0xC0); // sda, scl high
    writeWord_MD(0x100000, 0x40); // sda low, scl high
    writeWord_MD(0x100000, 0x00); // START
  }
  else if (eepType == 5) { // Codemasters
    writeWord_CM(0x180000, 0x00); // sda low, scl low
    writeWord_CM(0x180000, 0x02); // sda low, scl high
    writeWord_CM(0x180000, 0x03); // sda, scl high
    writeWord_CM(0x180000, 0x02); // sda low, scl high
    writeWord_CM(0x180000, 0x00); // START
  }
  else {
    writeWord_MD(0x100000, 0x00); // sda low, scl low
    writeWord_MD(0x100000, 0x03); // sda, scl high
    writeWord_MD(0x100000, 0x02); // sda low, scl high
    writeWord_MD(0x100000, 0x00); // START
  }
}

void EepromSet0() {
  if (eepType == 2) { // Acclaim Type 2
    writeWord_SDA(0x100000, 0x00); // sda low
    writeWord_SCL(0x100000, 0x01); // scl high
    writeWord_SDA(0x100000, 0x00); // sda low
    writeWord_SCL(0x100000, 0x00); // scl low
  }
  else if (eepType == 4) { // EA
    writeWord_MD(0x100000, 0x00); // sda low, scl low
    writeWord_MD(0x100000, 0x40); // sda low, scl high // 0
    writeWord_MD(0x100000, 0x00); // sda low, scl low
  }
  else if (eepType == 5) { // Codemasters
    writeWord_CM(0x180000, 0x00); // sda low, scl low
    writeWord_CM(0x180000, 0x02); // sda low, scl high // 0
    writeWord_CM(0x180000, 0x00); // sda low, scl low
  }
  else {
    writeWord_MD(0x100000, 0x00); // sda low, scl low
    writeWord_MD(0x100000, 0x02); // sda low, scl high // 0
    writeWord_MD(0x100000, 0x00); // sda low, scl low
  }
}

void EepromSet1() {
  if (eepType == 2) { // Acclaim Type 2
    writeWord_SDA(0x100000, 0x01); // sda high
    writeWord_SCL(0x100000, 0x01); // scl high
    writeWord_SDA(0x100000, 0x01); // sda high
    writeWord_SCL(0x100000, 0x00); // scl low
  }
  else if (eepType == 4) { // EA
    writeWord_MD(0x100000, 0x80); // sda high, scl low
    writeWord_MD(0x100000, 0xC0); // sda high, scl high // 1
    writeWord_MD(0x100000, 0x80); // sda high, scl low
    writeWord_MD(0x100000, 0x00); // sda low, scl low
  }
  else if (eepType == 5) { // Codemasters
    writeWord_CM(0x180000, 0x01); // sda high, scl low
    writeWord_CM(0x180000, 0x03); // sda high, scl high // 1
    writeWord_CM(0x180000, 0x01); // sda high, scl low
    writeWord_CM(0x180000, 0x00); // sda low, scl low
  }
  else {
    writeWord_MD(0x100000, 0x01); // sda high, scl low
    writeWord_MD(0x100000, 0x03); // sda high, scl high // 1
    writeWord_MD(0x100000, 0x01); // sda high, scl low
    writeWord_MD(0x100000, 0x00); // sda low, scl low
  }
}


void EepromDevice() { // 24C02+
  EepromSet1();
  EepromSet0();
  EepromSet1();
  EepromSet0();
}

void EepromSetDeviceAddress(word addrhi) { // 24C02+
  for (int i = 0; i < 3; i++) {
    if ((addrhi >> 2) & 0x1) // Bit is HIGH
      EepromSet1();
    else // Bit is LOW
      EepromSet0();
    addrhi <<= 1; // rotate to the next bit
  }
}

void EepromStatus() { // ACK
  byte eepStatus = 1;
  if (eepType == 1) { // Acclaim Type 1
    writeWord_MD(0x100000, 0x01); // sda high, scl low
    writeWord_MD(0x100000, 0x03); // sda high, scl high
    do {
      dataIn_MD();
      eepStatus = ((readWord_MD(0x100000) >> 1) & 0x1);
      dataOut_MD();
      delayMicroseconds(4);
    }
    while (eepStatus == 1);
    writeWord_MD(0x100000, 0x01); // sda high, scl low
  }
  else if (eepType == 2) { // Acclaim Type 2
    writeWord_SDA(0x100000, 0x01); // sda high
    writeWord_SCL(0x100000, 0x01); // scl high
    do {
      dataIn_MD();
      eepStatus = (readWord_MD(0x100000) & 0x1);
      dataOut_MD();
      delayMicroseconds(4);
    }
    while (eepStatus == 1);
    writeWord_SCL(0x100000, 0x00); // scl low
  }
  else if (eepType == 3) { // Capcom/Sega
    writeWord_MD(0x100000, 0x01); // sda high, scl low
    writeWord_MD(0x100000, 0x03); // sda high, scl high
    do {
      dataIn_MD();
      eepStatus = (readWord_MD(0x100000) & 0x1);
      dataOut_MD();
      delayMicroseconds(4);
    }
    while (eepStatus == 1);
    writeWord_MD(0x100000, 0x01); // sda high, scl low
  }
  else if (eepType == 4) { // EA
    writeWord_MD(0x100000, 0x80); // sda high, scl low
    writeWord_MD(0x100000, 0xC0); // sda high, scl high
    do {
      dataIn_MD();
      eepStatus = ((readWord_MD(0x100000) >> 7) & 0x1);
      dataOut_MD();
      delayMicroseconds(4);
    }
    while (eepStatus == 1);
    writeWord_MD(0x100000, 0x80); // sda high, scl low
  }
  else if (eepType == 5) { // Codemasters
    writeWord_CM(0x180000, 0x01); // sda high, scl low
    writeWord_CM(0x180000, 0x03); // sda high, scl high
    do {
      dataIn_MD();
      eepStatus = ((readWord_MD(0x1C0000) >> 7) & 0x1);
      dataOut_MD();
      delayMicroseconds(4);
    }
    while (eepStatus == 1);
    writeWord_CM(0x180000, 0x01); // sda high, scl low
  }
}

void EepromReadMode() {
  EepromSet1(); // READ
  EepromStatus(); // ACK
}

void EepromWriteMode() {
  EepromSet0(); // WRITE
  EepromStatus(); // ACK
}

void EepromReadData() {
  if (eepType == 1) { // Acclaim Type 1
    for (int i = 0; i < 8; i++) {
      writeWord_MD(0x100000, 0x03); // sda high, scl high
      dataIn_MD();
      eepbit[i] = ((readWord_MD(0x100000) >> 1) & 0x1); // Read 0x100000 with Mask 0x1 (bit 1)
      dataOut_MD();
      writeWord_MD(0x100000, 0x01); // sda high, scl low
    }
  }
  else if (eepType == 2) { // Acclaim Type 2
    for (int i = 0; i < 8; i++) {
      writeWord_SDA(0x100000, 0x01); // sda high
      writeWord_SCL(0x100000, 0x01); // scl high
      dataIn_MD();
      eepbit[i] = (readWord_MD(0x100000) & 0x1); // Read 0x100000 with Mask 0x1 (bit 0)
      dataOut_MD();
      writeWord_SDA(0x100000, 0x01); // sda high
      writeWord_SCL(0x100000, 0x00); // scl low
    }
  }
  else if (eepType == 3) { // Capcom/Sega
    for (int i = 0; i < 8; i++) {
      writeWord_MD(0x100000, 0x03); // sda high, scl high
      dataIn_MD();
      eepbit[i] = (readWord_MD(0x100000) & 0x1); // Read 0x100000 with Mask 0x1 (bit 0)
      dataOut_MD();
      writeWord_MD(0x100000, 0x01); // sda high, scl low
    }
  }
  else if (eepType == 4) { // EA
    for (int i = 0; i < 8; i++) {
      writeWord_MD(0x100000, 0xC0); // sda high, scl high
      dataIn_MD();
      eepbit[i] = ((readWord_MD(0x100000) >> 7) & 0x1); // Read 0x100000 with Mask (bit 7)
      dataOut_MD();
      writeWord_MD(0x100000, 0x80); // sda high, scl low
    }
  }
  else if (eepType == 5) { // Codemasters
    for (int i = 0; i < 8; i++) {
      writeWord_CM(0x180000, 0x03); // sda high, scl high
      dataIn_MD();
      eepbit[i] = ((readWord_MD(0x1C0000) >> 7) & 0x1); // Read 0x1C0000 with Mask 0x1 (bit 7)
      dataOut_MD();
      writeWord_CM(0x180000, 0x01); // sda high, scl low
    }
  }
}

void EepromWriteData(byte data) {
  for (int i = 0; i < 8; i++) {
    if ((data >> 7) & 0x1) // Bit is HIGH
      EepromSet1();
    else // Bit is LOW
      EepromSet0();
    data <<= 1; // rotate to the next bit
  }
  EepromStatus(); // ACK
}

void EepromFinish() {
  if (eepType == 2) { // Acclaim Type 2
    writeWord_SDA(0x100000, 0x00); // sda low
    writeWord_SCL(0x100000, 0x00); // scl low
    writeWord_SDA(0x100000, 0x01); // sda high
    writeWord_SCL(0x100000, 0x00); // scl low
    writeWord_SDA(0x100000, 0x01); // sda high
    writeWord_SCL(0x100000, 0x01); // scl high
    writeWord_SDA(0x100000, 0x01); // sda high
    writeWord_SCL(0x100000, 0x00); // scl low
    writeWord_SDA(0x100000, 0x00); // sda low
    writeWord_SCL(0x100000, 0x00); // scl low
  }
  else if (eepType == 4) { // EA
    writeWord_MD(0x100000, 0x00); // sda low, scl low
    writeWord_MD(0x100000, 0x80); // sda high, scl low
    writeWord_MD(0x100000, 0xC0); // sda high, scl high
    writeWord_MD(0x100000, 0x80); // sda high, scl low
    writeWord_MD(0x100000, 0x00); // sda low, scl low
  }
  else if (eepType == 5) { // Codemasters
    writeWord_CM(0x180000, 0x00); // sda low, scl low
    writeWord_CM(0x180000, 0x01); // sda high, scl low
    writeWord_CM(0x180000, 0x03); // sda high, scl high
    writeWord_CM(0x180000, 0x01); // sda high, scl low
    writeWord_CM(0x180000, 0x00); // sda low, scl low
  }
  else {
    writeWord_MD(0x100000, 0x00); // sda low, scl low
    writeWord_MD(0x100000, 0x01); // sda high, scl low
    writeWord_MD(0x100000, 0x03); // sda high, scl high
    writeWord_MD(0x100000, 0x01); // sda high, scl low
    writeWord_MD(0x100000, 0x00); // sda low, scl low
  }
}

void EepromStop() {
  if (eepType == 2) { // Acclaim Type 2
    writeWord_SDA(0x100000, 0x00); // sda low
    writeWord_SCL(0x100000, 0x01); // scl high
    writeWord_SDA(0x100000, 0x01); // sda high
    writeWord_SCL(0x100000, 0x01); // scl high
    writeWord_SDA(0x100000, 0x01); // sda high
    writeWord_SCL(0x100000, 0x00); // scl low
    writeWord_SDA(0x100000, 0x00); // sda low
    writeWord_SCL(0x100000, 0x00); // scl low // STOP
  }
  else if (eepType == 4) { // EA
    writeWord_MD(0x100000, 0x00); // sda, scl low
    writeWord_MD(0x100000, 0x40); // sda low, scl high
    writeWord_MD(0x100000, 0xC0); // sda, scl high
    writeWord_MD(0x100000, 0x80); // sda high, scl low
    writeWord_MD(0x100000, 0x00); // STOP
  }
  else if (eepType == 5) { // Codemasters
    writeWord_CM(0x180000, 0x00); // sda low, scl low
    writeWord_CM(0x180000, 0x02); // sda low, scl high
    writeWord_CM(0x180000, 0x03); // sda, scl high
    writeWord_CM(0x180000, 0x01); // sda high, scl low
    writeWord_CM(0x180000, 0x00); // STOP
  }
  else {
    writeWord_MD(0x100000, 0x00); // sda, scl low
    writeWord_MD(0x100000, 0x02); // sda low, scl high
    writeWord_MD(0x100000, 0x03); // sda, scl high
    writeWord_MD(0x100000, 0x01); // sda high, scl low
    writeWord_MD(0x100000, 0x00); // STOP
  }
}

void EepromSetAddress(word address) {
  if (eepSize > 0x80) { // 24C02+
    for (int i = 0; i < 8; i++) {
      if ((address >> 7) & 0x1) // Bit is HIGH
        EepromSet1();
      else // Bit is LOW
        EepromSet0();
      address <<= 1; // rotate to the next bit
    }
    EepromStatus(); // ACK
  }
  else { // 24C01
    for (int i = 0; i < 7; i++) {
      if ((address >> 6) & 0x1) // Bit is HIGH
        EepromSet1();
      else // Bit is LOW
        EepromSet0();
      address <<= 1; // rotate to the next bit
    }
  }
}

void readEepromByte(word address) {
  addrhi = address >> 8;
  addrlo = address & 0xFF;
  dataOut_MD();
  if (eepType == 2)
    EepromInit(0); // Enable EEPROM
  EepromStart(); // START
  if (eepSize > 0x80) {
    EepromDevice(); // DEVICE [1010]
    if (eepSize > 0x800) { // MODE 3 [24C65]
      EepromSetDeviceAddress(0);
      EepromWriteMode();
      EepromSetAddress(addrhi); // ADDR [A15..A8]
    }
    else { // MODE 2 [24C02/24C08/24C16]
      EepromSetDeviceAddress(addrhi); // ADDR [A10..A8]
      EepromWriteMode();
    }
  }
  EepromSetAddress(addrlo);
  if (eepSize > 0x80) {
    EepromStart(); // START
    EepromDevice(); // DEVICE [1010]
    if (eepSize > 0x800) // MODE 3 [24C65]
      EepromSetDeviceAddress(0);
    else // MODE 2 [24C02/24C08/24C16]
      EepromSetDeviceAddress(addrhi); // ADDR [A10..A8]
  }
  EepromReadMode();
  EepromReadData();
  EepromFinish();
  EepromStop(); // STOP
  if (eepType == 2)
    EepromInit(1); // Disable EEPROM
  // OR 8 bits into byte
  eeptemp = eepbit[0] << 7 | eepbit[1] << 6 | eepbit[2] << 5 | eepbit[3] << 4 | eepbit[4] << 3 | eepbit[5] << 2 | eepbit[6] << 1 | eepbit[7];
  sdBuffer[addrlo] = eeptemp;
}

void writeEepromByte(word address) {
  addrhi = address >> 8;
  addrlo = address & 0xFF;
  eeptemp = sdBuffer[addrlo];
  dataOut_MD();
  if (eepType == 2)
    EepromInit(0); // Enable EEPROM
  EepromStart(); // START
  if (eepSize > 0x80) {
    EepromDevice(); // DEVICE [1010]
    if (eepSize > 0x800) { // MODE 3 [24C65]
      EepromSetDeviceAddress(0); // [A2-A0] = 000
      EepromWriteMode(); // WRITE
      EepromSetAddress(addrhi); // ADDR [A15-A8]
    }
    else { // MODE 2 [24C02/24C08/24C16]
      EepromSetDeviceAddress(addrhi); // ADDR [A10-A8]
      EepromWriteMode(); // WRITE
    }
    EepromSetAddress(addrlo);
  }
  else { // 24C01
    EepromSetAddress(addrlo);
    EepromWriteMode(); // WRITE
  }
  EepromWriteData(eeptemp);
  EepromStop(); // STOP
  if (eepType == 2)
    EepromInit(1); // Disable EEPROM
}

// Read EEPROM and save to the SD card
void readEEP_MD() {
  dataIn_MD();

  // Get name, add extension and convert to char array for sd lib
  strcpy(fileName, romName);
  strcat(fileName, ".eep");

  // create a new folder for the save file
  EEPROM_readAnything(0, foldern);
  sd.chdir();
  sprintf(folder, "MD/SAVE/%s/%d", romName, foldern);
  sd.mkdir(folder, true);
  sd.chdir(folder);

  // write new folder number back to eeprom
  foldern = foldern + 1;
  EEPROM_writeAnything(0, foldern);

  println_Msg(F("Reading..."));
  display_Update();

  // Open file on sd card
  if (!myFile.open(fileName, O_RDWR | O_CREAT)) {
    print_Error(F("SD Error"), true);
  }
  if (eepSize > 0x100) { // 24C04+
    for (word currByte = 0; currByte < eepSize; currByte += 256) {
      print_Msg(F("*"));
      display_Update();
      for (int i = 0; i < 256; i++) {
        readEepromByte(currByte + i);
      }
      myFile.write(sdBuffer, 256);
    }
  }
  else { // 24C01/24C02
    for (word currByte = 0; currByte < eepSize; currByte++) {
      if ((currByte != 0) && ((currByte + 1) % 16 == 0)) {
        print_Msg(F("*"));
        display_Update();
      }
      readEepromByte(currByte);
    }
    myFile.write(sdBuffer, eepSize);
  }
  // Close the file:
  myFile.close();
  println_Msg(F(""));
  display_Clear();
  print_Msg(F("Saved to "));
  print_Msg(folder);

  display_Update();
}

void writeEEP_MD() {
  dataOut_MD();

  // Create filepath
  sprintf(filePath, "%s/%s", filePath, fileName);
  println_Msg(F("Writing..."));
  println_Msg(filePath);
  display_Update();

  // Open file on sd card
  if (myFile.open(filePath, O_READ)) {
    if (eepSize > 0x100) { // 24C04+
      for (word currByte = 0; currByte < eepSize; currByte += 256) {
        myFile.read(sdBuffer, 256);
        for (int i = 0; i < 256; i++) {
          writeEepromByte(currByte + i);
          delay(50); // DELAY NEEDED
        }
        print_Msg(F("."));
        display_Update();
      }
    }
    else { // 24C01/24C02
      myFile.read(sdBuffer, eepSize);
      for (word currByte = 0; currByte < eepSize; currByte++) {
        writeEepromByte(currByte);
        print_Msg(F("."));
        if ((currByte != 0) && ((currByte + 1) % 64 == 0))
          println_Msg(F(""));
        display_Update(); // ON SERIAL = delay(100)
      }
    }
    // Close the file:
    myFile.close();
    println_Msg(F(""));
    display_Clear();
    println_Msg(F("Done"));
    display_Update();
  }
  else {
    print_Error(F("SD Error"), true);
  }
  dataIn_MD();
}

//******************************************
// CD Backup RAM Functions
//******************************************
void readBram_MD() {
  dataIn_MD();

  // Get name, add extension and convert to char array for sd lib
  strcpy(fileName, "Cart.brm");

  // create a new folder for the save file
  EEPROM_readAnything(0, foldern);
  sd.chdir();
  sprintf(folder, "MD/RAM/%d", foldern);
  sd.mkdir(folder, true);
  sd.chdir(folder);

  // write new folder number back to eeprom
  foldern = foldern + 1;
  EEPROM_writeAnything(0, foldern);

  println_Msg(F("Reading..."));
  display_Update();

  // Open file on sd card
  if (!myFile.open(fileName, O_RDWR | O_CREAT)) {
    print_Error(F("SD Error"), true);
  }

  for (unsigned long currByte = 0; currByte < bramSize; currByte += 512) {
    for (int i = 0; i < 512; i++) {
      sdBuffer[i] = readWord_MD(0x300000 + currByte + i);
    }
    myFile.write(sdBuffer, 512);
  }

  // Close the file:
  myFile.close();
  println_Msg(F(""));
  display_Clear();
  print_Msg(F("Saved to "));
  print_Msg(folder);

  display_Update();
}

void writeBram_MD() {
  dataOut_MD();

  // Create filepath
  sprintf(filePath, "%s/%s", filePath, fileName);
  println_Msg(F("Writing..."));
  println_Msg(filePath);
  display_Update();

  // Open file on sd card
  if (myFile.open(filePath, O_READ)) {

    // 0x700000-0x7FFFFF: Writes by /LWR latch D0; 1=RAM write enabled, 0=disabled
    writeWord_MD(0x380000, 1); // Enable BRAM Writes

    for (unsigned long currByte = 0; currByte < bramSize; currByte += 512) {
      myFile.read(sdBuffer, 512);
      for (int i = 0; i < 512; i++) {
        writeWord_MD(0x300000 + currByte + i, sdBuffer[i]);
      }
    }
    writeWord_MD(0x380000, 0); // Disable BRAM Writes
    // Close the file:
    myFile.close();
    println_Msg(F(""));
    display_Clear();
    println_Msg(F("Done"));
    display_Update();
  }
  else {
    print_Error(F("SD Error"), true);
  }
  dataIn_MD();
}

//******************************************
// Realtec Mapper Functions
//******************************************
void writeRealtec(unsigned long address, byte value) { // Realtec 0x404000 (UPPER)/0x400000 (LOWER)
  dataOut_MD();
  PORTF = address & 0xFF; // 0x00 ADDR A0-A7
  PORTK = (address >> 8) & 0xFF; // ADDR A8-A15
  PORTL = (address >> 16) & 0xFF; //0x20 ADDR A16-A23
  PORTA = 0x00; // DATA D8-D15
  PORTH |= (1 << 0); // /RES HIGH

  PORTH |= (1 << 3); // CE HIGH
  PORTC = value;
  PORTH &= ~(1 << 4) & ~(1 << 5); // /UDSW + /LDSW LOW
  PORTH |= (1 << 4) | (1 << 5); // /UDSW + /LDSW HIGH
  dataIn_MD();
}

void readRealtec_MD() {
  // Set control
  dataIn_MD();

  // Get name, add extension and convert to char array for sd lib
  strcpy(fileName, romName);
  strcat(fileName, ".MD");

  // create a new folder
  EEPROM_readAnything(10, foldern);
  sprintf(folder, "MD/ROM/%s/%d", romName, foldern);
  sd.mkdir(folder, true);
  sd.chdir(folder);

  display_Clear();
  print_Msg(F("Saving to "));
  print_Msg(folder);
  println_Msg(F("/..."));
  display_Update();

  // write new folder number back to eeprom
  foldern = foldern + 1;
  EEPROM_writeAnything(10, foldern);

  // Open file on sd card
  if (!myFile.open(fileName, O_RDWR | O_CREAT)) {
    print_Error(F("SD Error"), true);
  }

  // Realtec Registers
  writeWord_MD(0x201000, 4); // Number of 128K Blocks 0x402000 (0x201000)
  writeRealtec(0x200000, 1); // ROM Lower Address 0x400000 (0x200000)
  writeRealtec(0x202000, 0); // ROM Upper Address 0x404000 (0x202000)

  word d = 0;
  for (unsigned long currBuffer = 0; currBuffer < cartSize / 2; currBuffer += 256) {
    // Blink led
    if (currBuffer % 16384 == 0)
      PORTB ^= (1 << 4);

    for (int currWord = 0; currWord < 256; currWord++) {
      word myWord = readWord_MD(currBuffer + currWord);
      // Split word into two bytes
      // Left
      sdBuffer[d] = (( myWord >> 8 ) & 0xFF);
      // Right
      sdBuffer[d + 1] = (myWord & 0xFF);
      d += 2;
    }
    myFile.write(sdBuffer, 512);
    d = 0;
  }
  // Close the file:
  myFile.close();
}

//******************************************
// End of File
//******************************************
