<?php

function get_bytes($bitfile, $bytefile)
{
	// read bitstring and parse into bytes
	//$outbitstringfname = $tapefname.".bits.txt";
	$headerfile = $bytefile.".h";
	$outbitstringfname = $bitfile;
	$bitstring = file_get_contents($outbitstringfname);
	$numbits = strlen($bitstring);
	$binfilename = $bytefile;
	$binhandle = fopen($binfilename, "wb");
	$headerhandle = fopen($headerfile, "w");
	
	// write start of header file
	fwrite($headerhandle, "prog_uchar loadprogram[] PROGMEM = \n");
	fwrite($headerhandle, "{\n");

	print "$numbits bits.\n";
	$curr_bit = 1;
	$address = 256;
	$biterrors = 0;
	$bytecolumn = 0;
	while ($curr_bit < $numbits)
	{
		$startbit = $curr_bit;
		// get current bit
		//$thisendbit = $curr_bit + 8;
		//$bytestr = substr($bitstring, $curr_bit, 8);
		$low_nibble = substr($bitstring, $curr_bit, 4);
		$high_nibble = substr($bitstring, $curr_bit+4, 4);
		$between = substr($bitstring, $curr_bit+8, 3);
		
		//$bytestr = $high_nibble.$low_nibble;
		$bytestr = strrev($low_nibble.$high_nibble);
		
		$decimal = bindec($bytestr);
		$octal = decoct($decimal);
		
		$address_hi = floor($address / 256);
		$address_lo = $address % 256;
		
		$address_hi_oct = decoct($address_hi);
		$address_lo_oct = decoct($address_lo);
		
		// check for consistency in the stop bits
		// should be 110
		
		// increment byte;
		$curr_bit += 8;
		// increment parity, stop, start
		$curr_bit += 3;
		$errorstr = "";
		if ($between != "110" && strlen($between) == 3)
		{
			$biterrors++;
			print "error: ";
			print "$address_hi_oct $address_lo_oct:\t\t$low_nibble $high_nibble $between\t($octal)\t$startbit\t$errorstr\n";
			
			if ($between == "111" || $between == "101" || $between == "100")
			{
				$errorstr = "*($between)";
				// missing bit
				// insert it
				$between = "110";
				// and step back 1 bit
				$curr_bit--;
			}
			elseif (strlen($between) == 3)
			{
				print "error in bitstream ($between), couldn't finish";
				die;
			}
		}
		
		//print "$between\n";
		print "$address_hi_oct $address_lo_oct:\t\t$low_nibble $high_nibble $between\t($octal)\t$startbit\t$errorstr\n";
		
		// get binary version of current data
		$binchar = pack("C", $decimal);
		fwrite($binhandle, $binchar);
		
		// write hex char
		fwrite($headerhandle, "0x".dechex($decimal).",\t");
		$bytecolumn++;   
		if ($bytecolumn == 8)
		{
			fwrite($headerhandle, "\n");
			$bytecolumn = 0;
		}
		   
		//print "$address_hi_oct $address_lo_oct\t$bytestr\t$octal\t$decimal\n";
		
		
		
		$address++;
	}
	
	fwrite($headerhandle, "\n};");
	fclose($headerhandle);
	
	print "there were $biterrors bit errors.\n";
}
	
	
function find_zero_crossings($infile)
{
    $datfile = $infile.".dat";
    $zerocrossingfile = $infile.".zero.txt";
	$timingfile = $infile.".timing.txt";
	$bitfile = $infile.".bits.txt";
	$handle = fopen($datfile, "r");
	$handle_out = fopen($timingfile, "w");
	$handle_bits = fopen($bitfile, "w");
    $zerohandle = fopen($zerocrossingfile, "w");
	
	// skip to a real sample
	$started = false;
	$lastsample = 0;
	$lasttime = 0;
	$lastzerotime = 0;
	//$mid_freq = (2975 + 2125) / 2;
	$mid_freq = 2500;
	$lastfreq = 1;
	$lastfreqstart = 0;
	$error0 = 0;
	$error1 = 0;
	$num0 = 0;
	$num1 = 0;
	$totalbits = 0;
    
    
	while ($line = fgets($handle))
	{
		if (strpos($line, ";") === false)
		{
			$trline = trim($line);
			$parts = explode(" ", $trline);
			// get sample value
			$sample = $parts[sizeof($parts) - 1];
			$time = $parts[0];
			
			//print "$time $sample\n";
			
			if ($started)
			{
				// are the samples different signs?
				if (($lastsample < 0 && $sample > 0) ||
					($lastsample > 0 && $sample < 0))
				{
					// interpolate to find zero crossing
					$dt = ($sample * ($time - $lasttime)) / ($sample - $lastsample);
					$t_intersection = $time - $dt;
					
					$difftime = $t_intersection - $lastzerotime;
					$curr_freq = 1.0 / (2 * $difftime);
					
					if ($curr_freq > $mid_freq)
					{
						//print "$t_intersection\t$difftime\t$curr_freq\n";
                        fwrite($zerohandle, "$t_intersection\t$difftime\t$curr_freq\n");
						$thisfreq = 0;
					}
					else
					{
						//print "$t_intersection\t$difftime\t$curr_freq\n";
                        fwrite($zerohandle, "$t_intersection\t$difftime\t$curr_freq\n");
						$thisfreq = 1;
					}
					
					if ($thisfreq != $lastfreq)
					{
						$freq_duration = $lastzerotime - $lastfreqstart;
						$bits = $freq_duration * 1100;
						$bits_round = round($bits);
						$error = $bits - $bits_round;
						
						$bitstr = sprintf("%0.2f", $bits);
						
						
						if ($bits < 100)
						{
							fwrite($handle_out, "$lastfreq\t$lastfreqstart\t$lastzerotime\t[$bits_round]\t$bitstr\t$error\t$freq_duration\n");
							
                            /*
							if (abs($error) > 0.3 || $bits_round == 0)
							{
								print "err: $lastfreq\t$lastfreqstart\t$lastzerotime\t[$bits_round]\t$bitstr\t$error\t$freq_duration\n";
							}
                            */
							
							$totalbits += $bits_round;
							for ($bb = 0; $bb < $bits_round; $bb++)
							{
								fwrite($handle_bits, $lastfreq);
							}
						}
						
						$lastfreq = $thisfreq;
						$lastfreqstart = $lastzerotime;
					}
					
					$lastzerotime = $t_intersection;
					
					
				}
				elseif ($sample == 0)
				{
					$difftime = $time - $lastzerotime;					
					$curr_freq = 1.0 / (2 * $difftime);
					
					if ($curr_freq > $mid_freq)
					{
						//print "$time\t$difftime\t$curr_freq\n";
                        fwrite($zerohandle, "$time\t$difftime\t$curr_freq\n");
						$thisfreq = 0;
					}
					else
					{
						//print "$time\t$difftime\t$curr_freq\n";
                        fwrite($zerohandle, "$time\t$difftime\t$curr_freq\n");
						$thisfreq = 1;
					}
					
					if ($thisfreq != $lastfreq)
					{
						$freq_duration = $lastzerotime - $lastfreqstart;
						$bits = $freq_duration * 1100;
						$bits_round = round($bits);
						$error = $bits - $bits_round;
						
						$bitstr = sprintf("%0.2f", $bits);
						
						if ($bits < 100)
						{
							fwrite($handle_out, "$lastfreq\t$lastfreqstart\t$lastzerotime\t[$bits_round]\t$bitstr\t$error\t$freq_duration\n");
							
                            /*
							if (abs($error) > 0.3 || $bits_round == 0)
							{
								print "err: $lastfreq\t$lastfreqstart\t$lastzerotime\t[$bits_round]\t$bitstr\t$error\t$freq_duration\n";
							}
                            */
							
							$totalbits = $bits_round;
							for ($bb = 0; $bb < $bits_round; $bb++)
							{
								fwrite($handle_bits, $lastfreq);
							}
						}
						
						$lastfreq = $thisfreq;
						$lastfreqstart = $lastzerotime;
					}
					
					$lastzerotime = $time;
				}
				
				$lastsample = $sample;
				$lasttime = $time;
			}
			else
			{
				if ($sample >= 0.5)
				{
					$started = true;
					$lastsample = $sample;
					$lasttime = $time;
					
					$lastfreqstart = $time;
				}
			}
			
		}
	}
	
	fclose($handle);
	fclose($handle_out);
	fclose($handle_bits);
    fclose($zerohandle);
	
	print "total bits: $totalbits\n";
}


function get_bytes_new($zerofile, $headerfile, $binfile, $programdata = false)
{

    $headerhandle = fopen($headerfile, "w");
    $binhandle = fopen($binfile, "wb");

    $hi_freq = 2975;
    $lo_freq = 2125;
    //$baudrate = 1100;
    $baudrate = 1105;
    $mid_freq = (2975 + 2125) / 2;
    $freq_diff = ($hi_freq - $lo_freq) / 2;
    $handle = fopen($zerofile, "r");
    
    fwrite($headerhandle, "prog_uchar loadprogram[] PROGMEM =\n");
    fwrite($headerhandle, "{\n");
    
    // first skip ahead to first zero (start) bit
    $line = fgets($handle);
    list($time, $duration, $freq) = explode("\t", $line);
    //print "$time, $duration, $freq\n";
    
    // look for zero bit (hi freq)
    while ($freq < $mid_freq)
    {
        $line = fgets($handle);
        list($time, $duration, $freq) = explode("\t", $line);
    }
    print "$time, $duration, $freq\n";
    
    
    $stop_addr_hi_oct = 0;
    $stop_addr_lo_oct = 0;
    $addr_hi_oct = 1;
    $addr_lo_oct = 0;
    $byte = 0;
    $headerbyte = 0;
    while ($addr_hi_oct != $stop_addr_hi_oct || $addr_lo_oct != $stop_addr_lo_oct)
    //for ($byte = 0; $byte < 1000; $byte++)
    {
    
        $addr_lo = $byte % 256;
        $addr_hi = (($byte - $addr_lo) / 256) + 1;
        
        $addr_hi_oct = decoct($addr_hi);
        $addr_lo_oct = decoct($addr_lo);
    
        // get a byte
        $thisbytestarttime = $time - $duration;
        // find byte end - stop bit + 8 data bits
        $thisbyteend = (9/$baudrate);
    
        $bittime = $thisbytestarttime;
        $bitvotes = array();
        
        for ($bit = 0; $bit < 9; $bit++)
        {
            $thisvotes = array(0,0);
            $bitendtime = $bittime + 1/$baudrate;
            //print "bit $bit ends $bitendtime\n";
            while ($time < $bitendtime)
            {
                // determine current bit
                if ($freq > $mid_freq)
                {
                    // this is a 0
                    $thisvotes[0]++;
                }
                else
                {
                    $thisvotes[1]++;
                }

                $line = fgets($handle);
                list($time, $duration, $freq) = explode("\t", $line);
            }
            
            $bitvotes[] = $thisvotes;
            $bittime = $bitendtime;
        }
        
        $bitstring = "";
        for ($bit = 8; $bit >= 0; $bit--)
        {
            //print "bit $bit: ";
            //print_r($bitvotes[$bit]);
            
            if ($bit >= 1)
            {
                if ($bitvotes[$bit][0] > $bitvotes[$bit][1])
                {
                    $bitstring .= "0";
                }
                else
                {
                    $bitstring .= "1";
                }
            }
        }
        
        // reconstruct 
        $dec = bindec($bitstring);
        $oct = decoct($dec);
        $hex = dechex($dec);
        
        
        if ($addr_hi_oct == 1 && $addr_lo_oct == 0 && $oct != 123 && $programdata == false)
        {
            // skip this byte
            print "$addr_hi_oct $addr_lo_oct\t$bitstring\t$oct\t($dec)\t";
            print "bad byte, skipping\n";
            
            while ($freq < $mid_freq)
            {
                $line = fgets($handle);
                list($time, $duration, $freq) = explode("\t", $line);
            }
        }
        else
        {
            print "$addr_hi_oct $addr_lo_oct\t$bitstring\t$oct\t($dec)\t";
            
            // write byte to header file
            fwrite($headerhandle, "0x$hex,\t");
            $thisbyte = pack("C", $dec);
            fwrite($binhandle, $thisbyte);
            $headerbyte++;
            if ($headerbyte == 8)
            {
                fwrite($headerhandle, "\n");
                $headerbyte = 0;
            }
            
            if ($programdata == false)
            {
                if ($addr_hi_oct == 1 && $addr_lo_oct == 32)
                {
                    $stop_addr_lo_oct = $oct;
                }
                else if ($addr_hi_oct == 1 && $addr_lo_oct == 33)
                {
                    $stop_addr_hi_oct = $oct;
                }
            }
            
            
            if ($addr_hi_oct == $stop_addr_hi_oct && $addr_lo_oct == $stop_addr_lo_oct)
            {
                break;
            }
            // skip to the stop bits if we are early
            while ($freq > $mid_freq)
            {
                $line = fgets($handle);
                list($time, $duration, $freq) = explode("\t", $line);
            }
            
            // go to the end of the stop bits
            $stopbittime = $time - $duration;
            while ($freq < $mid_freq)
            {
                $line = fgets($handle);
                list($time, $duration, $freq) = explode("\t", $line);
            }
            
            $stopbitduration = $time - $duration - $stopbittime;
            $stopbits = $stopbitduration / (1/$baudrate);
            
            $error = abs(2 - $stopbits);
            
            print "stop bit duration: $stopbitduration ($stopbits)";
            if ($error > 0.3)
            {
                print " **";
            }
            
            print "\n";
            $byte++;
        }
        
    }
    
    
    print "$stop_addr_hi_oct $stop_addr_lo_oct\n";
    fwrite($headerhandle, "};\n");
    fclose($headerhandle);
    fclose($binhandle);
}

	
$fname = $argv[1];

if (sizeof($argv) > 2)
{
    $program = true;
}
else
{
    $program = false;
}

$datfile = $fname.".dat";
find_zero_crossings($fname);
$zerofile = $fname.".zero.txt";
$headerfile = $fname.".h";
$binfile = $fname.".bin";

get_bytes_new($zerofile, $headerfile, $binfile, $program);


//$bitfile = $fname.".bits.txt";
//$bytefile = $fname.".bin";
//get_bytes($bitfile, $bytefile);
	
?>
