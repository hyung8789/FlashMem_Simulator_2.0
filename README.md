# FlashMem_Simulator_2.0
NAND Flash Memory Simulater for Block level Mapping Method with 2 Types of Mapping Table, Hybrid Mapping Method (Log algorithm - 1:2 Block level mapping with Dynamic Table)

	1) Static Table : Mapping table initially all correspond 1 : 1 (Logical Sector or Block Num -> Physical Sector or Block Num)
	2) Dynamic Table : Mapping table initially all empty (non-assigned)

※ Additional implementation

	- Garbage Collecter
		=> Because the cost of erasing operations is greater than reading and writing, 
		freeing up space for recording during idle time or through garbage collection at an appropriate time.

	- Trace function
		=> Performance test through write operation by specific pattern.	
<br></br>

<h3><strong>< Build Option ></strong></h3><br>

- Modify this predefined macro as you want

| Predefined Macro | For What |
|:---:|:---|
| DEBUG_MODE | Trace all possible error situations (0 : Not Use, 1 : Use) |
| BLOCK_TRACE_MODE | Trace wear-leveling per block for all physical blocks (0 : Not Use, 1 : Use) |
| SPARE_BLOCK_RATIO | The rate of spare blocks to be managed by the system for the total number of blocks (Direct data non-recordable blocks) (Default : 0.08, 8%) |
| VICTIM_BLOCK_QUEUE_RATIO | Set the size of the Victim Block Queue to the ratio size of the total number of blocks in the flash memory generated (Default : 0.5, 50%) |

<br></br>

<h3><strong>< Block Mapping Method, Hybrid Mapping Method Command List ></strong></h1>

| Command | Action |
|---|:---:|
| init x or i x | Create x MB Storage File |
| read LSN or r LSN | Read data at Logical Sector Num(LSN) Position |
| write LSN data or w LSN data | Write data at Logical Sector Num(LSN) Position |
| change | Change Mapping Method |
| print | Print Mapping Table(LSN -> PSN or LBN -> PBN) |
| trace | Write test with "W [TAB] PSN" format file |
| vqprint | Output Current Victim Block Queue |
| info | Output Current flash memory information |

<br></br>

<h3><strong>< Normal Mode (with not use Any Mapping Method) Command List ></strong></h3>
  
| Command | Action |
|---|:---:|
| init x or i x | Create x MB Storage File |
| read PSN or r PSN | Read data at Physical Sector Num(PSN) Position |
| write PSN data or w PSN data | Write data at Physical Sector Num(PSN) Position |
| erase PBN or e PBN | Erase data at Physical Block Num(PBN) Position |
| change | Change Mapping Method |
| info | Output Current flash memory information |

<br></br>

<h3><strong>< Bugs and needs to be improvement ></strong></h3><br>
1) In Hybrid Mapping, all physical spaces are written with valid data, no more new data can be written but overwrite action must can be performed in current data<br>
2) According to 1), Block Level Mapping Table assigned with overflowed(not valid) PBN num<br>
3) Block wear-leveling trace mode : not yet implemented<br>
4) Exception for Spare Block Table : case that not yet processed Victim Blocks are assigned to Spare Block Table<br>
5) Garbage Collector Scheduling Algorithm must be improved<br>
<br></br>

<h3><strong>< References ></strong></h3><br>
- A survey of Flash Translation Layer : <br>https://www.sciencedirect.com/science/article/abs/pii/S1383762109000356<br>
- A SPACE-EFFICIENT FLASH TRANSLATION LAYER FOR COMPACTFLASH SYSTEMS : <br>https://ieeexplore.ieee.org/document/1010143<br>
- Micron Technical Note - Garbage Collection : <br>https://www.micron.com/-/media/client/global/Documents/Products/Technical%20Note/NAND%20Flash/tn2960_garbage_collection_slc_nand.ashx<br>
- RRWL: Round Robin-Based Wear Leveling Using Block Erase Table for Flash Memory : <br>https://search.ieice.org/bin/summary.php?id=e100-d_5_1124
