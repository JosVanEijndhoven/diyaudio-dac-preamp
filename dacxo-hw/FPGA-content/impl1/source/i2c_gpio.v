//////////////////////////////////////////////////////////////////////
//
// i2cSlave
// design without extra clk input,
// according to http://dlbeer.co.nz/articles/i2c.html
//
//////////////////////////////////////////////////////////////////////
`timescale 1ns / 1ps

  
 
module i2cSlave (
  rst_p, // active high!
  sda, scl,
  myReg0,
  myReg1,
//  myReg2,
//  myReg3,
  myReg4,
  myReg5,
//  myReg6,
//  myReg7
  dbg
);
parameter [6:0] i2c_address = 7'h10;

input rst_p;
inout sda;
input scl;
output reg [7:0] myReg0 = 8'h00;
output reg [7:0] myReg1 = 8'h00;
//output [7:0] myReg2;
//output [7:0] myReg3;
input [7:0] myReg4;
input [7:0] myReg5;
//input [7:0] myReg6;
//input [7:0] myReg7;
output [5:0] dbg;

//////// i2c Output buffering
reg     sda_out = 1'b1;
assign  sda = sda_out ? 1'bz : 1'b0;

//////// Start frame detection
reg             start_detect = 0;
reg             start_resetter = 0;
wire            start_rst = rst_p | start_resetter;

always @ (posedge start_rst or negedge sda)
begin
        if (start_rst)
                start_detect <= 1'b0;
        else
                start_detect <= scl;
end

always @ (posedge rst_p or posedge scl)
begin
        if (rst_p)
                start_resetter <= 1'b0;
        else
                start_resetter <= start_detect;
end

//////// Stop frame detection
reg             stop_detect = 0;
reg             stop_resetter = 0;
wire            stop_rst = rst_p | stop_resetter;

always @ (posedge stop_rst or posedge sda)
begin   
        if (stop_rst)
                stop_detect <= 1'b0;
        else
                stop_detect <= scl;
end

always @ (posedge rst_p or posedge scl)
begin   
        if (rst_p)
                stop_resetter <= 1'b0;
        else
                stop_resetter <= stop_detect;
end

//////// Latching input serial data
reg [3:0]       bit_counter = 4'h0;

wire            lsb_bit = (bit_counter == 4'h7) && !start_detect;
wire            ack_bit = (bit_counter == 4'h8) && !start_detect;

always @ (negedge scl)
begin
        if (ack_bit || start_detect)
                bit_counter <= 4'h0;
        else
                bit_counter <= bit_counter + 4'h1;
end

reg [7:0]       input_shift;
wire            address_detect = (input_shift[7:1] == i2c_address);
wire            read_write_bit = input_shift[0];

always @ (posedge scl) //posedge rst_p or posedge scl)
begin
        if (!ack_bit)
                input_shift <= {input_shift[6:0], sda};
end

reg             master_ack;
always @ (posedge scl)
begin
        if (ack_bit)
                master_ack <= ~sda;
end

//////// I2C protocol state diagram, separating address, read, write data
parameter [2:0] STATE_IDLE      = 3'h0,
                STATE_DEV_ADDR  = 3'h1,
                STATE_READ      = 3'h2,
                STATE_IDX_PTR   = 3'h3,
                STATE_WRITE     = 3'h4;

reg [2:0]       state = STATE_IDLE;
wire            write_strobe = (state == STATE_WRITE) && ack_bit;

//always @ (posedge rst_p or negedge scl)
always @ (negedge scl)
begin
//        if (rst_p)
//                state <= STATE_IDLE;
//        else
		if (start_detect)
                state <= STATE_DEV_ADDR;
        else if (ack_bit)
        begin
                case (state)
                STATE_DEV_ADDR:
                        if (!address_detect)
                                state <= STATE_IDLE;
                        else if (read_write_bit)
                                state <= STATE_READ;
                        else
                                state <= STATE_IDX_PTR;

                STATE_READ:
                        if (master_ack)
                                state <= STATE_READ;
                        else
                                state <= STATE_IDLE;

                STATE_IDX_PTR:
                        state <= STATE_WRITE;

                STATE_WRITE:
                        state <= STATE_WRITE;
						
				default: // in particular when STATE_IDLE:
						state <= STATE_IDLE;
                endcase
        end
end

//////// Index to selected register
reg [7:0] index_pointer = 8'h00;
//always @ (posedge rst_p or negedge scl)
always @ (negedge scl)
begin
//        if (rst_p)
//                index_pointer <= 8'h00;
//        else
//		if (stop_detect)
//                index_pointer <= 8'h00;
//        else 
		if (ack_bit)
        begin
                if (state == STATE_IDX_PTR)
                        index_pointer <= input_shift;
                else
                        index_pointer <= index_pointer + 8'h01;
        end
end

//////// GPIO register transfers
//always @ (posedge rst_p or negedge scl)
always @ (negedge scl)
begin
//    if (rst_p)
//    begin
//        myReg0 <= 8'h00;
//        myReg1 <= 8'h00;
//    end
//    else
	if (write_strobe)
    begin
		case (index_pointer)
		8'h30: myReg0 <= input_shift;
        8'h31: myReg1 <= input_shift;
		endcase
	end
end

reg [7:0] output_shift;
always @ (negedge scl)
begin   
    if (lsb_bit)
    begin   
        case (index_pointer)
        8'h30: output_shift <= myReg0;
        8'h31: output_shift <= myReg1;
        8'h34: output_shift <= myReg4;
        8'h35: output_shift <= myReg5;
	    default: output_shift <= 8'hca;
        endcase
    end
    else
        output_shift <= {output_shift[6:0], 1'b0};
end

//////// serial output driver
always @ (negedge scl) //posedge rst_p or negedge scl)
begin   
//        if (rst_p)
//                sda_out <= 1'b1;
//        else
		if (start_detect)
                sda_out <= 1'b1;
        else if (lsb_bit)
        begin   
                sda_out <=
                    !(((state == STATE_DEV_ADDR) && address_detect) ||
                      (state == STATE_IDX_PTR) ||
                      (state == STATE_WRITE));
        end
        else if (ack_bit)
        begin
                // Deliver the first bit of the next slave-to-master
                // transfer, if applicable.
                if (((state == STATE_READ) && master_ack) ||
                    ((state == STATE_DEV_ADDR) &&
                        address_detect && read_write_bit))
                        sda_out <= output_shift[7];
                else
                        sda_out <= 1'b1;
        end
        else if (state == STATE_READ)
                sda_out <= output_shift[7];
        else
                sda_out <= 1'b1;
end

//assign dbg = {state, start_detect, address_detect, ack_bit};
// just prior to the READ state, the 2 lsbs of output_shift are both 1.
// in the first cycle of READ, following falling edge of sck, the two lsb had shift, making then '10'.
assign dbg = {state, start_detect, address_detect, ack_bit};
endmodule