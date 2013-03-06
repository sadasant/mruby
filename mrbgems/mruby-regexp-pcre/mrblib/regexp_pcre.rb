class Regexp
  attr_reader :options
  attr_reader :source
  attr_reader :last_match

  def self.quote(str)
    self.escape(str)
  end

  def self.compile(*args)
    self.new(*args)
  end

  def self.last_match
    @last_match
  end

  def =~(str)
    return nil unless str

    m = self.match(str)
    if m
      m.begin(0)
    else
      nil
    end
  end

  def ===(str)
    unless str.is_a? String
      if str.is_a? Symbol
        str = str.to_s
      else
        return false
      end
    end
    self.match(str) != nil
  end

  def casefold?
    (@options & IGNORECASE) > 0
  end

  def self.escape(str)
    escape_table = {
      "\ " => '\\ ', # '?\ ' is a space
      "["  => '\\[',
      "]"  => '\\]', 
      "{"  => '\\{', 
      "}"  => '\\}', 
      "("  => '\\(', 
      ")"  => '\\)', 
      "|"  => '\\|', 
      "-"  => '\\-', 
      "*"  => '\\*', 
      "."  => '\\.', 
      "\\" => '\\\\',
      "?"  => '\\?', 
      "+"  => '\\+', 
      "^"  => '\\^', 
      "$"  => '\\$', 
      "#"  => '\\#', 
      "\n" => '\\n', 
      "\r" => '\\r', 
      "\f" => '\\f', 
      "\t" => '\\t', 
      "\v" => '\\v', 
    }

    s = ""
    str.each_char do |c|
      if escape_table[c]
        s += escape_table[c]
      else
        s += c
      end
    end
    s
  end

  def named_captures
    h = {}
    if @names
      @names.each do |k, v|
        h[k.to_s] = [v + 1]
      end
    end
    h
  end

  def names
    if @names
      ar = Array.new(@names.size)
      @names.each do |k, v|
        ar[v] = k.to_s
      end
      ar
    else
      []
    end
  end

  # private
  def name_push(name, index)
    @names ||= {}
    @names[name.to_sym] = index - 1
  end
end

class MatchData
  attr_reader :length
  attr_reader :regexp
  attr_reader :string

  def [](n)
    # XXX: if n is_a? Range
    # XXX: when we have 2nd argument...
    if n < 0
      n += @length
      return nil if n < 0
    elsif n >= @length
      return nil
    end
    b = self.begin(n)
    e = self.end(n)
    @string[b, e-b]
  end

  def begin(index)
    @data[index][:start] if @data && @data[index]
  end

  def captures
    self.to_a[1, @length]
  end

  def end(index)
    @data[index][:finish] if @data && @data[index]
  end

  def offset(n)
    [self.begin(n), self.end(n)]
  end

  def post_match
    @string[self.end(0), @string.length]
  end

  def pre_match
    @string[0, self.begin(0)]
  end

  def push(start = nil, finish = nil)
    @data.push({start: start, finish: finish})
  end

  def to_a
    a = []
    @length.times { |i| a << self[i] }
    a
  end

  def to_s
    self[0]
  end

  def matched_area
    x = @data[0][:start]
    y = @data[0][:finish]
    @string[x, y]
  end

  def inspect
    capts = captures
    if capts.empty?
      "#<MatchData \"#{matched_area}\">"
    else
      idx = 0 
      capts.map! {|capture| "#{idx += 1}:#{capture.inspect}"}
      "#<MatchData \"#{matched_area}\" #{capts.join(" ")}>"
    end 
  end

  alias size length

  #def names
  #def values_at
end
