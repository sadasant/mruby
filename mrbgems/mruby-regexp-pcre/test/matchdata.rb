assert('MatchData') do
  MatchData.class == Class and
  MatchData.superclass == Object
end

assert('MatchData#[]') do
  m = /(foo)(bar)(BAZ)?/.match("foobarbaz")
  p m[-2]
  m[0] == "foobar" and m[1] == "foo" and m[2] == "bar" and m[3] == nil and
  m[4] == nil and m[-2] == "bar"
end


