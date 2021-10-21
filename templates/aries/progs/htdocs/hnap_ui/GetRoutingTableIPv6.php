<? include "/htdocs/phplib/html.php";
if($Remove_XML_Head_Tail != 1)	{HTML_hnap_200_header();}

include "/htdocs/phplib/trace.php";
require "/etc/templates/hnap/GetRoutingTable.php";

$routing_list = getRoutingTable("IPv6");

$line_count = cut_count ($routing_list, "\n") -1;

/* findStringAndGetNextParameter("200 100 300", "100", " "); => return 300 */
function findStringAndGetNextParameter($str, $needle, $delimiter)
{
	if (strstr($str, $delimiter) == "")
	{
		return "";
	}
	$item_number = cut_count($str, $delimiter);
	$i = 0;
	while ($i < $item_number)
	{
		if (cut($str, $i, $delimiter) == $needle)
		{
			return cut($str, $i + 1, $delimiter);
		}
		$i ++;
	}
	return "";
}

?>
<? if($Remove_XML_Head_Tail != 1)	{HTML_hnap_xml_header();}?>
		<GetRoutingTableIPv6Response xmlns="http://purenetworks.com/HNAP1/">
			<GetRoutingTableIPv6Result>OK</GetRoutingTableIPv6Result>
			<RoutingTableIPv6List>
<?
$line_number = 0;

while ($line_number < $line_count)
{
	// Init the string.
	$DistNetwork = "";
	$Prefix = "";
	$Gateway = "";
	$Metric = "";
	$Interface = "";

	$line = cut($routing_list, $line_number, "\n");

	//Pharse dist address and prefix length.
	$dest_addr = cut ($line, 0, " ");
	if ($dest_addr == "")
	{
		$line_number ++;
		continue;
	}
	else if ($dest_addr == "default")
	{
		$DistNetwork = "::";
		$Prefix = "0";
	}
	else
	{
		$DistNetwork = cut ($dest_addr, 0, "/");
		$Prefix = cut ($dest_addr, 1, "/");
	}

	// Pharse next hop
	$Gateway = findStringAndGetNextParameter($line, "via", " ");

	// Pharse Metric
	$Metric = findStringAndGetNextParameter($line, "metric", " ");
	if ($Metric == "")
	{
		$Metric = "0";
	}

	// Pharse interface
	$Interface = findStringAndGetNextParameter($line, "dev", " ");

	// Pharsing Completed! I'm going to echo the result.
	echo "\t\t\t\t\<RoutingTableIPv6Info\>\n";
	echo "\t\t\t\t\t\<DistNetwork\>".$DistNetwork."\</DistNetwork\>\n";
	echo "\t\t\t\t\t\<Prefix\>".$Prefix."\</Prefix\>\n";
	echo "\t\t\t\t\t\<Gateway\>".$Gateway."\</Gateway\>\n";
	echo "\t\t\t\t\t\<Metric\>".$Metric."\</Metric\>\n";
	echo "\t\t\t\t\t\<Interface\>".$Interface."\</Interface\>\n";
	echo "\t\t\t\t\</RoutingTableIPv6Info\>\n";

	$line_number ++;
}
?>			</RoutingTableIPv6List>
		</GetRoutingTableIPv6Response>
<? if($Remove_XML_Head_Tail != 1)	{HTML_hnap_xml_tail();}?>
