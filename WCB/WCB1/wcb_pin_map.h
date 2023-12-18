//////////////////////////////// 
// Wireless Communincation Board (WCB) Pinout
//////////////////////////////// 

#ifndef wcb_pin_map.h
#define wcb_pin_map.h
#endif

// #define BOARD_T7
#define BOARD_S3

#ifdef BOARD_T7
//////////////////////////////// 
// This pinout is for a WROOM32
////////////////////////////////

#define UP_0  				      0  //  // Unused Pin (Must be LOW to boot)
#define SERIAL_TX_PIN       1  //  // Serial 1 Tx Pin
#define UP2		              2  //  // (must be left floating or LOW to enter flashing mode)
#define SERIAL_RX_PIN       3  //  // Serial 1 Rx Pin
#define SERIAL4_TX_PIN      4  //  // Serial 4 Tx Pin
#define UP_5	              5  //  // Unused Pin (must be HIGH during boot)
#define UP_6                6  //  // connected to SPI flash
#define UP_7                7  //  // connected to SPI flash
#define UP_8                8  //  // connected to SPI flash
#define UP_9                9  //  // connected to SPI flash
#define UP_10               10 //  // connected to SPI flash
#define UP_11               11 //  // connected to SPI flash
#define UP_12			          12 //  // Unused Pin (must be LOW during boot)
#define UP_13               13 //  // Unused Pin
#define UP_14               14 //  // Unused Pin
#define UP_15       		    15 //  // Unused Pin (must be HIGH during boot)
#define UP_16               16 //  // Unused Pin
#define UP_17               17 //  // Unused Pin
#define SERIAL2_RX_PIN	    18 //  // Serial 2 Rx Pin
#define UP_19               19 //  // ESP32 Status NeoPixel Pin
#define UP_20               20 //  // LoRa Status NeoPixel Pin
#define SERIAL4_RX_PIN      21 //  // Serial 4 Rx Pin
#define SERIAL5_TX_PIN      22 //  // Serial 5 Tx Pin
#define SERIAL5_RX_PIN	    23 //  // Serial 5 Rx Pin 
#define UP_24               24 //  // nused Pin
#define SERIAL3_RX_PIN      25 //  // Serial 3 Rx Pin
#define SERIAL2_TX_PIN	    26 //  // Serial 2 Tx Pin
#define SERIAL3_TX_PIN	    27 //  // Serial 3 Tx Pin
#define UP_28               28 //  // Unused Pin
#define UP_29               29 //  // Unused Pin
#define UP_30               30 //  // Unused Pin
#define UP_31 	            31 //  // TUnused Pin
#define UP_32			  	      32 //  // Unused Pin
#define UP_33	 	 	  	      33 //  // Unused Pin
#define UP_34 	            34 //  // Unused Pin - input only (Can be used for Rx) // Use pull-down resistor
#define UP_35               35 //  // Unused Pin - input only (Can be used for Rx) // Use pull-down resistor
#define UP_36               36 //  // Unused Pin - input only (Can be used for Rx) // Use pull-down resistor
#define UP_28               37 //  // Unused Pin - input only (Can be used for Rx) // Use pull-down resistor
#define UP_28               38 //  // Unused Pin - input only (Can be used for Rx) // Use pull-down resistor
#define UP_39               39 //  // Unused Pin - input only (Can be used for Rx) // Use pull-down resistor

#elif defined (BOARD_S3)

//////////////////////////////// 
// This pinout is for a WROOM-S3
////////////////////////////////
#define SERIAL_TX_PIN       43  //  // Serial 1 Tx Pin
#define SERIAL_RX_PIN       44  //  // Serial 1 Rx Pin
#define SERIAL2_TX_PIN      16  //  // Serial 4 Tx Pin
#define SERIAL2_RX_PIN	    18 //  // Serial 2 Rx Pin
#define SERIAL3_TX_PIN      3 //  // Serial 4 Rx Pin
#define SERIAL3_RX_PIN      14 //  // Serial 5 Tx Pin
#define SERIAL4_TX_PIN	    4 //  // Serial 5 Rx Pin 
#define SERIAL4_RX_PIN      13 //  // Serial 3 Rx Pin
#define SERIAL5_TX_PIN	    14 //  // Serial 2 Tx Pin
#define SERIAL5_RX_PIN	    8 //  // Serial 3 Tx Pin
#endif
