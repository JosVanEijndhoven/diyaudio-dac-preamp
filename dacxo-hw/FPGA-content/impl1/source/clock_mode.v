// Select clock mode - base frequency
// to match the input sample rate
// Jos van Eijndhoven, Sept. 2015
//

module clock_mode(
	rx_lock, rx_lrclk, sample_clk, GPO_0,
	rate_sel, is_lock, enbl_osc49M, enbl_osc45M,
	sample_clk_div2);
	
	parameter N_rate = 8;
	parameter N_hold = 5;
	input rx_lock, rx_lrclk, sample_clk;
	input [7:0] GPO_0;
	output reg [1:0] rate_sel;
	output reg is_lock;
	output enbl_osc49M, enbl_osc45M;
	output sample_clk_div2;
	
	// In master_mode, we set our own clock onto the dedicated I2S port
	wire master_mode = GPO_0[0];
	wire master_base48 = GPO_0[1];
	wire [1:0] master_rate = GPO_0[3:2];
	// note: in slave_mode, bits [3:2] address the spdiff input selector
	
	reg is_base48, is_rate2;
	reg [N_hold-1:0] hold_cnt;
	reg next_base48, next_rate2;
	wire [N_rate-1:0] rate;
	wire [11:0] slow_clk;
	
	// I2S master_mode bypasses the rate calculations as done below
	assign enbl_osc49M = master_mode ? master_base48 : is_base48;
	assign enbl_osc45M = !enbl_osc49M;
	assign sample_clk_div2 = slow_clk[0];
	
	initial
	begin
		is_base48 = 0;
		is_rate2 = 0;
		is_lock = 0;
		next_base48 = 0;
		next_rate2 = 0;
		hold_cnt = 0;
		rate_sel = 0;
	end
	
	always @(posedge sample_clk)
	begin
		if (master_mode)
			rate_sel <= master_rate;
		else // in slave mode, we do not support 176/192 kHz sample rates
			rate_sel <= is_rate2 ? 2'h2 : 2'h1;
	end

	// We need 6 bits to discriminate 44-48 or 88-96
	// Relative_rate input clock should be 5.6-6.1 MHz (2^7 x 44-48kHz), is 1 extra bit for 96 versus 48.
	// 'rate' is 8 bits wide to allow for some overflow counting
	clockdiv #(.N(12)) rate_clk( sample_clk, slow_clk);
	relative_rate #(.N(N_rate)) relative_rate( rx_lrclk, slow_clk[2], rate);
	
	wire prop_rate2  = rate <= 100; // 'prop_rate2' indicates a lrclk rate of 88 or 96kHz
	wire [N_rate-2:0] rate_shft = prop_rate2 ? rate[N_rate-2:0] : rate[N_rate-1:1];
	wire prop_base48 = is_base48 ? (rate_shft < 67) : (rate_shft < 61);
	wire decent_rate = (rate_shft > 54) && (rate_shft < 75);
	
	// be conservatie: only change clock mode if prediction is stable
	// Oscillator start-up time is 2msec.
	// Note, during oscillator switching this actual used clock will stall for a while
	always @(posedge slow_clk[11]) // rate is 11/12 kHz
	begin
		if (prop_base48 == next_base48 && prop_rate2 == next_rate2 && rx_lock && decent_rate)
			hold_cnt <= hold_cnt + 1; // check for counter overflow not really needed
		else
		begin
			next_base48 <= prop_base48;
			next_rate2  <= prop_rate2;
			hold_cnt    <= 0;
		end
		if (&hold_cnt) // long stable prediction, can change mode
		begin
			is_base48 <= next_base48;
			is_rate2  <= next_rate2;
		end
		is_lock <= (rx_lock && decent_rate) || master_mode;
	end	
endmodule // clock_mode
	
// determine rate = clk / rx_lrclk
module relative_rate( rx_lrclk, clk, rate);
	parameter N = 8;
	input rx_lrclk, clk;
	output reg [N-1:0] rate;
	
	reg [N-1:0] cnt;
	reg rx_lrclk_1, rx_lrclk_2;
	
	initial
	begin
		rate = 0;
		cnt = 0;
		rx_lrclk_1 = 0;
		rx_lrclk_2 = 0;
	end
		
	always @(posedge clk)
	begin 	// note: rx_lrclk is async relative to clk
		rx_lrclk_1 <= rx_lrclk;
		rx_lrclk_2 <= rx_lrclk_1;
		if (rx_lrclk_1 && ~rx_lrclk_2) // got rising edge on rx_lrclk
		begin
			rate <= cnt;
			cnt <= 0;
		end
		else if (~(&cnt)) // saturate to max number
			cnt <= cnt + 1;
	end
			
endmodule // relative_rate
	
	