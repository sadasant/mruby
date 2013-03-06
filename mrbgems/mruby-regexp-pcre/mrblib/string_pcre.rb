class String
  alias_method :old_sub, :sub
  def sub(*args, &blk)
    if args[0].class == String
      return blk ? old_sub(*args) { |x| blk.call(x) } : old_sub(*args)
    end

    begin
      m = args[0].match(self)
    rescue
      return self
    end
    return self if m.size == 0
    r = ''
    r += m.pre_match
    r += blk ? blk.call(m[0]) : args[1]
    r.to_sub_replacement!(m)
    r += m.post_match
    r
  end

  alias_method :old_gsub, :gsub
  def gsub(*args, &blk)
    if args[0].class.to_s == 'String'
      return blk ? old_gsub(*args) { |x| blk.call(x) } : old_gsub(*args)
    end

    s = self
    r = ""
    while true
      begin
        m = args[0].match(s)
      rescue
        break
      end

      break if !m || m.size == 0
      return r if m.end(0) == 0
      r += m.pre_match
      r += blk ? blk.call(m[0]) : args[1]
      r.to_sub_replacement!(m)
      s = m.post_match
    end
    r += s
    r
  end

  def =~(a)
    begin
      (a.class.to_s == 'String' ? Regexp.new(a.to_s) : a) =~ self
    rescue
      false
    end
  end

  # private
  def to_sub_replacement!(match)
    result = ""
    index = 0
    while index < self.length
      current = index
      while current < self.length && self[current] != '\\'
        current += 1
      end
      result += self[index, (current - index)]
      break if current == self.length

      if current == self.length - 1
        result += '\\'
        break
      end
      index = current + 1

      cap = self[index]

      case cap
      when "&"
        result += match[0]
      when "`"
        result += match.pre_match
      when "'"
        result += match.post_match
      when "+"
        result += match.captures.compact[-1].to_s
      when /[0-9]/
        result += match[cap.to_i].to_s
      when '\\'
        result += '\\'
      else
        result += '\\' + cap
      end
      index += 1
    end
    self.replace(result)
  end
end
