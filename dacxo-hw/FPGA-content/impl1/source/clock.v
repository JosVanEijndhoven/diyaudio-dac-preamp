// Clock generation
//
// Jos van Eijndhoven, May 2015

module clockdiv( clk, Q);
  parameter N=4;
  input clk;
  output reg [N-1:0] Q;
  
  integer i;
  reg [N-1:0] previs1;
  
  initial
    Q = 0;
	
  always @*
  begin
    previs1[0] = 1'b1;
	for (i=1; i<N; i=i+1)
	  previs1[i] = Q[i-1] && previs1[i-1];
  end
	
  always @(posedge clk)
  begin
	for (i=0; i<N; i=i+1)
	  if (previs1[i])
	    Q[i] <= !Q[i];
  end
  
  endmodule // clockdiv
  
  module clocktune (clk, adj_hi, adj_lo, adjclk_div4);
    parameter N=4;
	input clk, adj_hi, adj_lo;
	output reg adjclk_div4; // at posedge clk
	
	wire [N-1:0] Q;
	reg [1:0] phase;
	
	initial
	begin
	   phase = 0;
	   adjclk_div4 = 0;
	end
	
	// the adjclk_div4 output is similar to Q[1] (is clk div 4),
	// but shifted in time, in units of clk cycles.
	// 'phase' state creates a variable output lag in units of cycles
	clockdiv #(N) div ( clk, Q);
	
	//  clk:       1010 1010 1010 1010 1010 1010
	//  Q[1:0]:    0011 2233 0011 2233 0011 2233
	//  phase:     0000 0000 0011 1111 1111 1111
	//  Q_delay:   0011 2233 0022 3300 1122 3300
	//  clk_adj:   0000 1111 0011 1100 0011 1100

	wire [1:0] Q_delay = Q[1:0] + phase;
	
	always @(posedge clk)
	begin
		if (Q == 0 && adj_hi)
			phase <= phase + 2'h1; // reduce lag
		else if (Q == 0 && adj_lo)
		    phase <= phase - 2'h1; // increase lag
			
		adjclk_div4 <= Q_delay[1];
	end
endmodule // clocktune
	
module clockgen (clk, clk_div2, rate_sel, adj_hi, adj_lo, mclk, bitclk);
    parameter ADJ=10;
	input clk, clk_div2, adj_hi, adj_lo;
	input [1:0] rate_sel; //0: none, 1: 44/48k, 2: 88/96k, 3: 176/192k
	output reg mclk, bitclk;
	wire clk_adj;
	wire [1:0] clks;
	
	clocktune #(ADJ) clockadj // create clk_adj, which is about clk/4, adjusted with +/- 1/(2^^N)
    (clk, adj_hi, adj_lo, clk_adj);
	// the 176/192 kHz mode is reserved for I2S master mode only, where adj_xx is 0
      
	clockdiv #(2) do_mclk ( clk_adj, clks); // create divided-down versions of clk_adj
	
	always @*
	begin
	  case(rate_sel)
	  2'h0:  // unexpected value
      begin
 	     mclk = clks[0];// 0; maintain minimum of 11 or 12MHz mclk for pcm1792
		                // note that the pcm1792 requires the mclk to operate its i2c
		 bitclk = 0;
	  end
	  2'h1:  // 44 or 48kHz mode
      begin
 	     mclk = clks[0];//clks[0]; maintain minimum of 11 or 12MHz mclk for pcm1792
		 bitclk = clks[1];
	  end
	  2'h2: // 88 or 96kHz sample rate
	  begin
		 mclk = clk_adj;
		 bitclk = clks[0];
	  end
	  2'h3:
	  begin
		mclk = clk_div2; // master-mode only
		bitclk = clk_adj;
	  end
	  endcase
	end

endmodule // clockgen
	