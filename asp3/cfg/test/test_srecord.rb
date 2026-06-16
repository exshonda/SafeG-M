#!/usr/bin/env ruby -Eutf-8 -w
# -*- coding: utf-8 -*-

#
#	SRecord.rbのテスト
#

if $0 == __FILE__
  TOOL_ROOT = File.expand_path(File.dirname(__FILE__)) + "/"
  $LOAD_PATH.unshift(TOOL_ROOT)
end

require "test/unit"
require "../SRecord.rb"

class TC_SRecord < Test::Unit::TestCase
  def setup
    @romImage = SRecord.new("test_srecord.srec", :srec)
    #pp @romImage
  end

  def test_get_data
    assert_equal(@romImage.get_data(0x00, 1), "10")
    assert_equal(@romImage.get_data(0x20, 2), "2021")
    assert_equal(@romImage.get_data(0x40, 4), "30313233")
    assert_equal(@romImage.get_data(0x4e, 4), nil)
    assert_equal(@romImage.get_data(0x60, 1), nil)
  end

  def test_set_data
    @romImage.set_data(0x00, "80")
    assert_equal(@romImage.get_data(0x00, 1), "80")
    @romImage.set_data(0x20, "9091")
    assert_equal(@romImage.get_data(0x20, 2), "9091")
    @romImage.set_data(0x40, "a0a1a2a3")
    assert_equal(@romImage.get_data(0x40, 4), "a0a1a2a3")
    @romImage.set_data(0x3e, "b0b1b2b3")
    assert_equal(@romImage.instance_variable_get(:@dumpData).size, 4)
    assert_equal(@romImage.get_data(0x3e, 4), "b0b1b2b3")
    @romImage.set_data(0x50, "c0c1c2c3")
    assert_equal(@romImage.instance_variable_get(:@dumpData).size, 4)
    assert_equal(@romImage.get_data(0x50, 4), "c0c1c2c3")
    @romImage.set_data(0x08, "d0d1")
    assert_equal(@romImage.instance_variable_get(:@dumpData).size, 3)
    assert_equal(@romImage.get_data(0x08, 4), "d0d11a1b")
    #pp @romImage
  end

  def test_copy_data_1
    @romImage.copy_data(0x00, 0x0e, 4)
    assert_equal(@romImage.instance_variable_get(:@dumpData).size, 4)
    assert_equal(@romImage.get_data(0x0c, 6), "1c1d10111213")
    #pp @romImage
  end

  def test_copy_data_2
    @romImage.copy_data(0x00, 0x04, 8)
    assert_equal(@romImage.instance_variable_get(:@dumpData).size, 3)
    assert_equal(@romImage.get_data(0x04, 8), "1011121314151617")
    #pp @romImage
  end

  def test_copy_data_3
    @romImage.copy_data(0x04, 0x24, 8)
    assert_equal(@romImage.instance_variable_get(:@dumpData).size, 4)
    assert_equal(@romImage.get_data(0x24, 8), "1415161728291a1b")
    #pp @romImage
  end
end
