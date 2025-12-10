module cs8416_hwmode 
( input PIN_rst_in, PIN_RX_ad0rerr_in, PIN_RX_ad1audio_in,
        PIN_RX_rmck, PIN_RX_sdo, PIN_RX_sclk,  PIN_RX_lrck, 
 output PIN_RX_rst, PIN_RX_sel0,  PIN_RX_sel1, PIN_RX_ad0rerr_weak, PIN_RX_ad1audio_weak,
        PIN_RX_96k_weak, PIN_RX_rcbl_weak,
        PIN_RX_ad2gpo2_weak, PIN_RX_cgpo1_weak, PIN_RX_txgpo0_weak, PIN_RX_omck,
 input  [1:0]rx_sel,
 input  rx_enable,
 output rx_data, rx_lrclk, rx_sclk, rx_err, rx_lock
 );
 
 assign PIN_RX_rst = PIN_rst_in && rx_enable; // disable cs8416 in RPi I2S master mode
 assign PIN_RX_sel0 = rx_sel[0];
 assign PIN_RX_sel1 = rx_sel[1];
 assign PIN_RX_ad0rerr_weak = 1;
 assign PIN_RX_ad1audio_weak = 0; //SFSEL1=0
 assign PIN_RX_96k_weak = 1;
 assign PIN_RX_rcbl_weak = 1;
 assign PIN_RX_ad2gpo2_weak = 1;
 assign PIN_RX_cgpo1_weak = 0;  //SFSEL0=0 (SFSEL=00 -> audio format = left justified)
 assign PIN_RX_txgpo0_weak = 0;
 assign PIN_RX_omck = 0;
 
 assign rx_data  = PIN_RX_sdo;
 assign rx_lrclk = PIN_RX_lrck;
 assign rx_sclk = PIN_RX_sclk; // default: 64 x 44.1kHz = 2.8Mhz
 assign rx_err = PIN_RX_ad0rerr_in || PIN_RX_ad1audio_in; // 1=err, 0=OK
 assign rx_lock = !rx_err;
endmodule

// module cs8416_swmode 
// ( input PIN_rst_in, PIN_RX_ad0rerr_in, PIN_RX_ad1audio_in, PIN_RX_txgpo0_in, PIN_RX_cgpo1_weak,
        // PIN_RX_rmck, PIN_RX_sdo, PIN_RX_sclk,  PIN_RX_lrck, 
 // output PIN_RX_rst, PIN_RX_ad0rerr_weak, PIN_RX_ad1audio_weak, PIN_RX_ad2gpo2_weak, PIN_RX_omck,
 // output rx_data, rx_lrclk, rx_sclk, rx_lock, rx_hispeed, att20db
 // );
 
 // assign PIN_RX_rst = PIN_rst_in; // wakeup cs8416 just after fpga setup, assume pull-down before
 // assign PIN_RX_ad0rerr_weak = 0; // AD0,1,2 are all 0 ==> I2C chip address = 0x20
                                 // // PIN_RX_ad0rerr_in is not used in I2C mode. 
 // assign PIN_RX_ad1audio_weak = 0; // PIN_RX_ad1audio_in not used in I2C mode
 // assign PIN_RX_ad2gpo2_weak = 0; // weak pull-down on end of reset, GPO2 output unused

 // assign PIN_RX_omck = 0;
 
 // assign rx_data  = PIN_RX_sdo;
 // assign rx_lrclk = PIN_RX_lrck;
 // assign rx_sclk = PIN_RX_sclk; // default: 64 x 44.1kHz = 2.8Mhz
 // assign rx_lock = !PIN_RX_txgpo0_in; // select gpo0 to pass UNLOCK status
 // assign rx_hispeed = 0;
 // assign att20db = PIN_RX_cgpo1_weak;
// endmodule