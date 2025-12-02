// This tells Yosys how to convert internal gates to ToySim gates
module \$_AND_ (input A, input B, output Y); my_and g(Y, A, B); endmodule
module \$_OR_  (input A, input B, output Y); my_or  g(Y, A, B); endmodule
module \$_XOR_ (input A, input B, output Y); my_xor g(Y, A, B); endmodule
module \$_NOT_ (input A, output Y);          my_not g(Y, A);    endmodule

// Map Yosys internal DFF (Positive Edge) to our 'dff' module
module \$_DFF_P_ (input D, input C, output Q); 
    my_dff f(Q, C, D); 
endmodule