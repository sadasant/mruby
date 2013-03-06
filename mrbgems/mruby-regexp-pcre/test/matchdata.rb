assert('MatchData', '15.2.16.2') do
  MatchData.class == Class and
  MatchData.superclass == Object
end

assert('MatchData#[]', '15.2.16.3.1') do
  m = /(foo)(bar)(BAZ)?/.match("foobarbaz")
  m[0] == "foobar" and m[1] == "foo" and m[2] == "bar" and m[3] == nil and
  m[4] == nil and m[-2] == "bar"
end

assert('MatchData#begin', '15.2.16.3.2') do
  m = /(foo)(bar)(BAZ)?/.match("foobarbaz")
  m.begin(0) == 0 and m.begin(1) == 0 and m.begin(2) == 3 and m.begin(3) == nil
end

assert('MatchData#captures', '15.2.16.3.3') do
  m = /(foo)(bar)(BAZ)?/.match("foobarbaz")
  m.captures == ["foo", "bar", nil]
end

assert('MatchData#end', '15.2.16.3.4') do
  m = /(foo)(bar)(BAZ)?/.match("foobarbaz")
  m.end(0) == 6 and m.end(1) == 3 and m.end(2) == 6 and m.end(3) == nil
end

assert('MatchData#initialize_copy', '15.2.16.3.5') do
  m = /(foo)(bar)(BAZ)?/.match("foobarbaz")
  n = /a/.match("a").initialize_copy(m)
  m.to_a == n.to_a and m.regexp == n.regexp and m.string == n.string
end

assert('MatchData#length', '15.2.16.3.6') do
  m = /(foo)(bar)(BAZ)?/.match("foobarbaz")
  m.length == 4
end
