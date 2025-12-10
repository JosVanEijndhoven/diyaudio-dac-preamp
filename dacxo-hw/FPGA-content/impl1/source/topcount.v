module topcount 
(input PIN_ext4, // used as external reset input, active low, has resistor pull-up
                 // Note: external late reset would require software reset as well :-(
       PIN_ext5, // input with switch debounce, for now unused.
 input PIN_i2c_scl,
 inout PIN_i2c_sda, /* synthesis OPENDRAIN="ON" */
 // interface to cs8416 receiver
 input PIN_RX_ad0rerr_in, PIN_RX_ad1audio_in, 
        //PIN_RX_rmck,
		PIN_RX_sdo, PIN_RX_sclk,  PIN_RX_lrck, 
 output PIN_RX_rst, PIN_RX_ad0rerr_weak, PIN_RX_ad1audio_weak,
        PIN_RX_cgpo1_weak, PIN_RX_ad2gpo2_weak, PIN_RX_omck,
 // output in hw mode only:
 output PIN_RX_sel0,  PIN_RX_sel1, PIN_RX_96k_weak, PIN_RX_rcbl_weak, PIN_RX_txgpo0_weak,
 // input in sw mode only: PIN_RX_txgpo0_in, 
 
 // interface to audio output and DAC chips
 input  PIN_Vana,
 output reg PIN_tx_data, PIN_tx_lrclk, PIN_tx_bitclk, PIN_tx_mclk,
 output PIN_pcm_addr1, PIN_rst_dac,
 // interface to status LEDs
 output PIN_adj_hi, PIN_adj_lo, PIN_speed_hi, PIN_speed_lo, PIN_rcv_lock, PIN_rcv_audio,
 // interface to I2S audio input port where we are clock master
 output reg PIN_i2s_bitclk, PIN_i2s_lrclk,
 input PIN_i2s_data,
 // interface to xtal oscillators
 output XOinv, XO45sel, XO45en, XO49en,
 input sample_clk, //XO45_clk, XO49_clk, 
 // interface to I/O switches
 output PIN_ext1, PIN_ext2, PIN_ext3
 );
 
   wire rx_data, rx_lrclk, rx_sclk, rx_lock;
   wire almost_full, almost_empty, is_full, is_empty, overflow, underflow;
   wire clk_adj11M;
   wire tx_data, tx_lrclk, tx_bitclk, tx_mclk;
   wire is_lock, enbl_osc49M, enbl_osc45M, sample_clk_div2;
   wire [1:0] rate_sel; // 0: none, 1: 44/48kHz, 2: 88/96kHz, 3: 176/192kHz
   wire i2c_enable; // output of i2c, free for applicationm use for module control
   wire [7:0] GPO_0, GPO_1, GPI_0, GPI_1; // output and input bytes for i2c access
   reg adj_hi, adj_lo, nom_is_slow;
   wire [5:0] i2cdbg;
   wire master_mode = GPO_0[0];
   wire att20db = GPO_1[0];
   wire powerup = GPO_0[7];
 
   assign PIN_ext1      = att20db;   // when pin_ext1 is 1 then relay is powered and attenuation is off (0dB)
   assign PIN_ext2      = powerup;   // drive relay to power-up analog power supply
   assign PIN_ext3      = GPO_1[0];  // debug output
   assign PIN_adj_hi    = adj_hi || master_mode;
   assign PIN_adj_lo    = adj_lo || master_mode;
   assign PIN_speed_hi  = rate_sel[1];
   assign PIN_speed_lo  = rate_sel[0];
   assign PIN_rcv_lock  = rx_lock;
   assign PIN_rcv_audio = !(overflow || underflow);
   assign PIN_pcm_addr1 = 0;
   assign PIN_rst_dac   = PIN_ext4    && PIN_Vana; // stay in reset if Vana is low
   assign XO45en        = enbl_osc45M && PIN_Vana;
   assign XO49en        = enbl_osc49M && PIN_Vana;
   assign XO45sel       = 0; //test[1]; // hmm.. in current faulty PCB this must be connected to gnd.
   assign XOinv         = 0; //test[0]; // hmm.. in current faulty PCB this signal is ALSO connected to the sel input

   wire [1:0] master_rate = GPO_0[3:2]; // in master mode, these bits dictate the samplerate
   reg 	buf1_i2s_data, bufp_i2s_lrclk, buf1_i2s_lrclk, buf2_i2s_lrclk;
   wire rx_enable = !master_mode;
   wire [1:0] rx_sel = GPO_0[3:2]; // in slave mode, these bits provide the spdif input select
   assign GPI_0 = {almost_full, almost_empty, adj_lo, adj_hi, rate_sel, enbl_osc49M, rx_lock};
   assign GPI_1 = {2'h0, is_full, is_empty, rx_lock, PIN_ext4, PIN_ext5, PIN_Vana};

   initial
   begin
	   adj_hi = 0; //  use higher-then-nominal clockrate
	   adj_lo = 0; //  use lower-then-nominal clockrate
	   nom_is_slow = 0; //  nominal clock tends to fill-up fifo
	   PIN_i2s_bitclk = 0;
	   PIN_i2s_lrclk = 0;
	   buf1_i2s_data = 0;
	   bufp_i2s_lrclk = 0;
	   buf1_i2s_lrclk = 0;
	   buf2_i2s_lrclk = 0;
	   PIN_tx_data = 0;
	   PIN_tx_lrclk = 0;
	   PIN_tx_bitclk = 0;
	   PIN_tx_mclk = 0;
   end

   cs8416_hwmode cs8416
   ( PIN_ext4, PIN_RX_ad0rerr_in, PIN_RX_ad1audio_in, 
     PIN_RX_rmck, PIN_RX_sdo, PIN_RX_sclk,  PIN_RX_lrck, 
     PIN_RX_rst, PIN_RX_sel0,  PIN_RX_sel1, PIN_RX_ad0rerr_weak, PIN_RX_ad1audio_weak, PIN_RX_96k_weak, PIN_RX_rcbl_weak,
	 PIN_RX_ad2gpo2_weak, PIN_RX_cgpo1_weak, PIN_RX_txgpo0_weak, PIN_RX_omck,
     rx_sel, rx_enable,
	 rx_data, rx_lrclk, rx_sclk, rx_err, rx_lock
   );
// cs8416 in sw mode was used when the fpga did not have its own i2c slave for gpio register setup
//   cs8416_swmode cs8416
//   ( PIN_ext4, PIN_RX_ad0rerr_in, PIN_RX_ad1audio_in, PIN_RX_txgpo0_in, PIN_RX_cgpo1_weak,
//     PIN_RX_rmck, PIN_RX_sdo, PIN_RX_sclk,  PIN_RX_lrck, 
//     PIN_RX_rst, PIN_RX_ad0rerr_weak, PIN_RX_ad1audio_weak, PIN_RX_ad2gpo2_weak, PIN_RX_omck,
//     rx_data, rx_lrclk, rx_sclk, rx_lock, rx_hispeed, att20db
//   );

   clock_mode clock_mode
   ( rx_lock, rx_lrclk, sample_clk, GPO_0,
     rate_sel, is_lock, enbl_osc49M, enbl_osc45M,
	 sample_clk_div2
    );
   
   clockgen #(.ADJ(10)) clockgen // mclk is about sample_clk/4 (when is_rate2) or sample_clk/8
   ( sample_clk, sample_clk_div2, rate_sel, adj_hi, adj_lo,
     tx_mclk, tx_bitclk
   );
 
   audio_buffer audio_buffer
   ( rx_data, rx_lrclk, rx_sclk, rx_lock,
     tx_data, tx_lrclk, tx_bitclk,
     almost_full, almost_empty, is_full, is_empty, overflow, underflow
   );

   i2cSlave i2c_gpio
   ( !PIN_ext4, PIN_i2c_sda, PIN_i2c_scl,
     GPO_0, GPO_1, GPI_0, GPI_1, i2cdbg
   );

   // buffered I/O to I2S sound input where we are clock master
   always @(posedge sample_clk_div2)
   begin
	if (!master_mode || !PIN_Vana || (master_rate == 2'h0))
	begin
		PIN_i2s_bitclk <= 0;
		PIN_i2s_lrclk  <= 0;
	end
	else
	begin
		PIN_i2s_bitclk <= tx_bitclk;
		PIN_i2s_lrclk  <= tx_lrclk;
	end
   end
   // first (for safeness) half-bit-period delay of lrck
   always @(posedge PIN_i2s_bitclk)
       bufp_i2s_lrclk  <= PIN_i2s_lrclk;
   
   always @(negedge PIN_i2s_bitclk)
   begin
       buf1_i2s_data  <= PIN_i2s_data; // inbound data would be safe to clock on (late) negedge
	   buf1_i2s_lrclk <= bufp_i2s_lrclk; 
	   buf2_i2s_lrclk <= buf1_i2s_lrclk; // so data lags behind. That is not on our dac output
   end

   // buffered output to dac chips
   always @(posedge sample_clk)
   begin
		// without Vana power supply, keep outputs at 0
		PIN_tx_data   <= PIN_Vana && (master_mode ? buf1_i2s_data : tx_data); 
		PIN_tx_lrclk  <= PIN_Vana && (master_mode ? buf2_i2s_lrclk : tx_lrclk);
		PIN_tx_bitclk <= PIN_Vana && tx_bitclk;
		PIN_tx_mclk   <= PIN_Vana && tx_mclk;
   end
 
   // fine-tune clock rate selection from buffer filling
   always @(posedge tx_bitclk)
   begin
	   if (master_mode)
	   begin
		   adj_lo <= 0;
		   adj_hi <= 0;
	   end
	   else if (is_full)
	   begin
		   adj_lo <= 0;
		   adj_hi <= 1;
		   if (!adj_hi)
			   nom_is_slow <= 1; // nom speed fills-up fifo
	   end
	   else if (almost_full)
	   begin
		   adj_lo <= 0;
		   if (nom_is_slow) // better choose hi clock
			   adj_hi <= 1;
	   end
	   else if (is_empty)
	   begin
		   adj_hi <= 0;
		   adj_lo <= 1;
		   if (!adj_lo)
			   nom_is_slow <= 0; // nom speed drains fifo
	   end
	   else if (almost_empty)
	   begin
		   adj_hi <= 0;
		   if (!nom_is_slow) // better choose lo clock
			   adj_lo <= 1;		   
	   end
   end

 
endmodule
