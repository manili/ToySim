module sync_half_adder (
    input clk,
    input rst,
    input a,
    input b,
    output sum,
    output carry
);
    wire sum_tmp;
    wire carry_tmp;

    my_xor g1 (sum_tmp, a, b);
    my_and g2 (carry_tmp, a, b);

    my_dff_async f1 (sum, clk, rst, sum_tmp);
    my_dff_async f2 (carry, clk, rst, carry_tmp);
endmodule