#!/usr/bin/env/ruby

require 'socket'

CHR_TO_INT = {"y" => 3, "r" => 2, "g" => 1, "_" => 0}
def load_symbols(file)
  content = File.read(file)
  raw_symbols = content.split("\n\n")

  raw_symbols.inject({}) do |result, text|
    lines = text.split("\n").select { |s| s.size > 0 }
    symbol_name = lines[0].gsub(/[: ]/, '')

    if lines.count != 9
      raise RuntimeError, "Incorrect number of lines for symbol #{symbol_name}"
    end

    symbol_data = lines[1..-1].map do |l|
      l.gsub(' ', '').chars.map { |ch| CHR_TO_INT[ch] }
    end.flatten

    if symbol_data.size != 8*8
      raise RuntimeError, "Incorrect matrix size for symbol #{symbol_name}"
    end

    result[symbol_name] = symbol_data
    result
  end
end

SYMBOLS = load_symbols("./symbols.txt")
$socket = UNIXSocket.new("/tmp/cipi.socket")

def send_message(symbols)
  $socket.write symbols.map{ |s| SYMBOLS[s].map(&:chr) }.join("")
end

[["chr_h", "chr_e", "chr_l", "chr_l", "chr_o"],
 ["red", "red", "red", "red", "red"],
 ["green", "green", "green", "green", "green"],
 ["yellow", "yellow", "yellow", "yellow", "yellow"],
 ["red_cross", "red_cross", "ok", "smile", "green_arrow"],
 ["blank", "blank", "blank", "blank", "blank"]].each do |msg|

  send_message(msg)
  sleep(4)
end
