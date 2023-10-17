void kernel make_heatmap(global float* accumulation_buffer, global uint* render_buffer, global float* sample_count_reciprocal, global struct SceneData* scene_data)
{     
    uint colors[] =
    {
        854151,
    919687,
    984967,
    984968,
    1050504,
    1116040,
    1181577,
    1181577,
    1247113,
    1312649,
    1378186,
    1378186,
    1443722,
    1509259,
    1509259,
    1574539,
    1640075,
    1640076,
    1705612,
    1705612,
    1771149,
    1836685,
    1836685,
    1902221,
    1902222,
    1967758,
    1967758,
    2033294,
    2098831,
    2098831,
    2164367,
    2164367,
    2229904,
    2229904,
    2295440,
    2295440,
    2360977,
    2360721,
    2426257,
    2426257,
    2491793,
    2491794,
    2557330,
    2557330,
    2622866,
    2622867,
    2688403,
    2688403,
    2753939,
    2753939,
    2819476,
    2819476,
    2885012,
    2885012,
    2950548,
    2950549,
    3016085,
    3016085,
    3016085,
    3081622,
    3081622,
    3147158,
    3147158,
    3212694,
    3212694,
    3278231,
    3278231,
    3343767,
    3343767,
    3343767,
    3409304,
    3409048,
    3474584,
    3474584,
    3540120,
    3540121,
    3605657,
    3605657,
    3605657,
    3671193,
    3671193,
    3736730,
    3736730,
    3802266,
    3802266,
    3802266,
    3867803,
    3867803,
    3933339,
    3933339,
    3998875,
    3998875,
    3998876,
    4064412,
    4064412,
    4129948,
    4129948,
    4195484,
    4195485,
    4195485,
    4261021,
    4261021,
    4326301,
    4326301,
    4326302,
    4391838,
    4391838,
    4457374,
    4457374,
    4522910,
    4522910,
    4522911,
    4588447,
    4588447,
    4653983,
    4653983,
    4653983,
    4719519,
    4719520,
    4785056,
    4785056,
    4785056,
    4850592,
    4850592,
    4916128,
    4916129,
    4981665,
    4981409,
    4981409,
    5046945,
    5046945,
    5112481,
    5112482,
    5112482,
    5178018,
    5178018,
    5243554,
    5243554,
    5243554,
    5309090,
    5309091,
    5374627,
    5374627,
    5374627,
    5440163,
    5440163,
    5505699,
    5505699,
    5505699,
    5571236,
    5571236,
    5636516,
    5636516,
    5636516,
    5702052,
    5702052,
    5767588,
    5767588,
    5767589,
    5833125,
    5833125,
    5833125,
    5898661,
    5898661,
    5964197,
    5964197,
    5964197,
    6029733,
    6029734,
    6095270,
    6095270,
    6095270,
    6160806,
    6160806,
    6226342,
    6226342,
    6226342,
    6291878,
    6291878,
    6357414,
    6357158,
    6357159,
    6422695,
    6422695,
    6488231,
    6488231,
    6488231,
    6553767,
    6553767,
    6553767,
    6619303,
    6619303,
    6684839,
    6684839,
    6684839,
    6750375,
    6750375,
    6815912,
    6815912,
    6815912,
    6881448,
    6881448,
    6881448,
    6946984,
    6946984,
    7012520,
    7012520,
    7012520,
    7078056,
    7078056,
    7143592,
    7143592,
    7143592,
    7209128,
    7209128,
    7209128,
    7274664,
    7274664,
    7340200,
    7340200,
    7340200,
    7405736,
    7405736,
    7471272,
    7471528,
    7471528,
    7537064,
    7537064,
    7537064,
    7602600,
    7602600,
    7668136,
    7668136,
    7668136,
    7733672,
    7733672,
    7733672,
    7799208,
    7799208,
    7864744,
    7864744,
    7864744,
    7930536,
    7930536,
    7930536,
    7996072,
    7996072,
    8061608,
    8061608,
    8061608,
    8127144,
    8127144,
    8127400,
    8192936,
    8192936,
    8258472,
    8258472,
    8258472,
    8324008,
    8324008,
    8324008,
    8389800,
    8389799,
    8389799,
    8455335,
    8455335,
    8520871,
    8521127,
    8521127,
    8586663,
    8586663,
    8586663,
    8652199,
    8652455,
    8717991,
    8717991,
    8717991,
    8783526,
    8783526,
    8783782,
    8849318,
    8849318,
    8849318,
    8915110,
    8915110,
    8915110,
    8980646,
    8980646,
    9046437,
    9046437,
    9046437,
    9111973,
    9112229,
    9112229,
    9177765,
    9178021,
    9178021,
    9243557,
    9243556,
    9243812,
    9309348,
    9309348,
    9309348,
    9375140,
    9375140,
    9440676,
    9440675,
    9440931,
    9506467,
    9506467,
    9506723,
    9572259,
    9572259,
    9572259,
    9638050,
    9638050,
    9638050,
    9703586,
    9703842,
    9703842,
    9769378,
    9769633,
    9769633,
    9835169,
    9835169,
    9835425,
    9900961,
    9900960,
    9900960,
    9966752,
    9966752,
    9966752,
    10032544,
    10032543,
    10032543,
    10098079,
    10098335,
    10098335,
    10163871,
    10163870,
    10164126,
    10229662,
    10229662,
    10229918,
    10295454,
    10295453,
    10295453,
    10361245,
    10361245,
    10361245,
    10427036,
    10427036,
    10427036,
    10492572,
    10492828,
    10492828,
    10558363,
    10558363,
    10558619,
    10624155,
    10624155,
    10624410,
    10624410,
    10689946,
    10689946,
    10690202,
    10755737,
    10755737,
    10755993,
    10821529,
    10821529,
    10821528,
    10887320,
    10887320,
    10887320,
    10952855,
    10953111,
    10953111,
    10953111,
    11018903,
    11018902,
    11018902,
    11084438,
    11084694,
    11084694,
    11150229,
    11150485,
    11150485,
    11150485,
    11216020,
    11216276,
    11216276,
    11281812,
    11282068,
    11282067,
    11347603,
    11347603,
    11347859,
    11347858,
    11413394,
    11413650,
    11413650,
    11479186,
    11479185,
    11479441,
    11479441,
    11544977,
    11544976,
    11545232,
    11610768,
    11610768,
    11611024,
    11611023,
    11676559,
    11676559,
    11676815,
    11742350,
    11742350,
    11742606,
    11742606,
    11808141,
    11808141,
    11808397,
    11873933,
    11873933,
    11874188,
    11874188,
    11939724,
    11939724,
    11939979,
    11939979,
    12005515,
    12005771,
    12005770,
    12071306,
    12071306,
    12071562,
    12071562,
    12137097,
    12137353,
    12137353,
    12137353,
    12202888,
    12203144,
    12203144,
    12268680,
    12268679,
    12268935,
    12268935,
    12334471,
    12334726,
    12334726,
    12334726,
    12400262,
    12400518,
    12400517,
    12400517,
    12466309,
    12466309,
    12466308,
    12466308,
    12532100,
    12532100,
    12532099,
    12532355,
    12597891,
    12597891,
    12597890,
    12663682,
    12663682,
    12663682,
    12663938,
    12729473,
    12729473,
    12729473,
    12729729,
    12795264,
    12795264,
    12795264,
    12795520,
    12861055,
    12861055,
    12861311,
    12861311,
    12861311,
    12926846,
    12927102,
    12927102,
    12927102,
    12992893,
    12992893,
    12992893,
    12992893,
    13058684,
    13058684,
    13058684,
    13058940,
    13124476,
    13124475,
    13124475,
    13124731,
    13190267,
    13190266,
    13190266,
    13190522,
    13256058,
    13256057,
    13256313,
    13256313,
    13256313,
    13321849,
    13322104,
    13322104,
    13322104,
    13387896,
    13387895,
    13387895,
    13387895,
    13453687,
    13453686,
    13453686,
    13453942,
    13519478,
    13519478,
    13519477,
    13519733,
    13519733,
    13585269,
    13585524,
    13585524,
    13585524,
    13651060,
    13651316,
    13651315,
    13651315,
    13651571,
    13717107,
    13717106,
    13717106,
    13717362,
    13782898,
    13782898,
    13782897,
    13783153,
    13783153,
    13848689,
    13848944,
    13848944,
    13848944,
    13914480,
    13914736,
    13914735,
    13914735,
    13914991,
    13980527,
    13980526,
    13980526,
    13980782,
    13980782,
    14046318,
    14046573,
    14046573,
    14046573,
    14112109,
    14112364,
    14112364,
    14112364,
    14112620,
    14178156,
    14178155,
    14178155,
    14178411,
    14178411,
    14243947,
    14244202,
    14244202,
    14244202,
    14244202,
    14309993,
    14309993,
    14309993,
    14310249,
    14310249,
    14375784,
    14375784,
    14376040,
    14376040,
    14376039,
    14441831,
    14441831,
    14441831,
    14441831,
    14507622,
    14507622,
    14507622,
    14507878,
    14507878,
    14507877,
    14573413,
    14573669,
    14573669,
    14573668,
    14573924,
    14639460,
    14639460,
    14639716,
    14639715,
    14639715,
    14705251,
    14705507,
    14705507,
    14705506,
    14705762,
    14771298,
    14771298,
    14771297,
    14771553,
    14771553,
    14837089,
    14837345,
    14837344,
    14837344,
    14837600,
    14903136,
    14903136,
    14903135,
    14903391,
    14903391,
    14903391,
    14969182,
    14969182,
    14969182,
    14969182,
    14969438,
    15034973,
    15034973,
    15035229,
    15035229,
    15035229,
    15035484,
    15101020,
    15101020,
    15101020,
    15101275,
    15101275,
    15166811,
    15167067,
    15167067,
    15167066,
    15167322,
    15167322,
    15232858,
    15232858,
    15233113,
    15233113,
    15233113,
    15233369,
    15298904,
    15298904,
    15299160,
    15299160,
    15299160,
    15299159,
    15364951,
    15364951,
    15364951,
    15365207,
    15365206,
    15365206,
    15430998,
    15430998,
    15430998,
    15431253,
    15431253,
    15431253,
    15496789,
    15497044,
    15497044,
    15497044,
    15497300,
    15497300,
    15562835,
    15563091,
    15563091,
    15563091,
    15563347,
    15563346,
    15628882,
    15628882,
    15629138,
    15629137,
    15629137,
    15629393,
    15629393,
    15694929,
    15695184,
    15695184,
    15695184,
    15695440,
    15695440,
    15695439,
    15761231,
    15761231,
    15761231,
    15761486,
    15761486,
    15761486,
    15761486,
    15827278,
    15827277,
    15827277,
    15827533,
    15827533,
    15827532,
    15827788,
    15893324,
    15893324,
    15893580,
    15893579,
    15893579,
    15893835,
    15893835,
    15959371,
    15959626,
    15959626,
    15959626,
    15959882,
    15959881,
    15959881,
    15960137,
    16025673,
    16025673,
    16025928,
    16025928,
    16025928,
    16026184,
    16026184,
    16091719,
    16091975,
    16091975,
    16091975,
    16092230,
    16092230,
    16092230,
    16092486,
    16092486,
    16158021,
    16158277,
    16158277,
    16158277,
    16158533,
    16158532,
    16158532,
    16158788,
    16224324,
    16224323,
    16224579,
    16224579,
    16224579,
    16224835,
    16224834,
    16224834,
    16225090,
    16225090,
    16290625,
    16290881,
    16290881,
    16290881,
    16291137,
    16291136,
    16291136,
    16291392,
    16291392,
    16291392,
    16357183,
    16357183,
    16357183,
    16357439,
    16357438,
    16357438,
    16357694,
    16357694,
    16357694,
    16357949,
    16423485,
    16423485,
    16423741,
    16423741,
    16423996,
    16423996,
    16423996,
    16424252,
    16424251,
    16424251,
    16424507,
    16424507,
    16424507,
    16490298,
    16490298,
    16490298,
    16490554,
    16490554,
    16490553,
    16490809,
    16490809,
    16491065,
    16491065,
    16491064,
    16491320,
    16491320,
    16491320,
    16557111,
    16557111,
    16557111,
    16557367,
    16557367,
    16557366,
    16557622,
    16557622,
    16557878,
    16557878,
    16557877,
    16558133,
    16558133,
    16558133,
    16558389,
    16558388,
    16558388,
    16624180,
    16624180,
    16624436,
    16624435,
    16624435,
    16624691,
    16624691,
    16624690,
    16624946,
    16624946,
    16625202,
    16625202,
    16625201,
    16625457,
    16625457,
    16625457,
    16625713,
    16625713,
    16625968,
    16625968,
    16625968,
    16626224,
    16626224,
    16626223,
    16626479,
    16626479,
    16626735,
    16626735,
    16626734,
    16626990,
    16626990,
    16692526,
    16692782,
    16692781,
    16693037,
    16693037,
    16693037,
    16693293,
    16693293,
    16693292,
    16693548,
    16693548,
    16693804,
    16693804,
    16693804,
    16694059,
    16694059,
    16694315,
    16694315,
    16694315,
    16694571,
    16694570,
    16694570,
    16694826,
    16694826,
    16695082,
    16695082,
    16695081,
    16695337,
    16629801,
    16630057,
    16630057,
    16630057,
    16630313,
    16630312,
    16630568,
    16630568,
    16630568,
    16630824,
    16630824,
    16631080,
    16631079,
    16631079,
    16631335,
    16631335,
    16631591,
    16631591,
    16631591,
    16631847,
    16631846,
    16632102,
    16632102,
    16632102,
    16632358,
    16632358,
    16632614,
    16632614,
    16632614,
    16567334,
    16567334,
    16567589,
    16567589,
    16567589,
    16567845,
    16567845,
    16568101,
    16568101,
    16568101,
    16568357,
    16568357,
    16568613,
    16568613,
    16568613,
    16568869,
    16503333,
    16503589,
    16503588,
    16503588,
    16503844,
    16503844,
    16504100,
    16504100,
    16504356,
    16504356,
    16504356,
    16504612,
    16439076,
    16439332,
    16439332,
    16439332,
    16439588,
    16439588,
    16439844,
    16439844,
    16439844,
    16440100,
    16374564,
    16374820,
    16374820,
    16375076,
    16375077,
    16375077,
    16375333,
    16375333,
    16375589,
    16310053,
    16310309,
    16310309,
    16310309,
    16310565,
    16310565,
    16310821,
    16310821,
    16245285,
    16245541,
    16245541,
    16245797,
    16245797,
    16246054,
    16246054,
    16246054,
    16180774,
    16180774,
    16181030,
    16181030,
    16181286,
    16181286,
    16181286,
    16116006,
    16116006,
    16116262,
    16116263,
    16116519,
    16116519,
    16050983,
    16051239,
    16051239,
    16051495,
    16051495,
    16051751,
    15986215,
    15986215,
    15986471,
    15986471,
    15986727,
    15986727,
    15986983,
    15921447,
    15921447,
    15921703,
    15921703,
    15921959,
    15921958,
    15856678,
    15856678,
    15856678,
    15856934,
    15856933,
    15857189,
    15791653,
    15791908,
    15791908,
    15791907,
    15792163,
    15792162,
    15792417,
    };
    // Taken from https://www.kennethmoreland.com/color-advice/

	int x = get_global_id(0);
	int y = get_global_id(1);

	uint pixel_dest = (x + y * scene_data->resolution_x);

	float3 color = (float3)(
		accumulation_buffer[pixel_dest * 4 + 0], 
		accumulation_buffer[pixel_dest * 4 + 1], 
		accumulation_buffer[pixel_dest * 4 + 2]);

    color *= *sample_count_reciprocal;

	//color = (float3)(1.0f, 0.0f, 0.0f);

    int heatmap_idx = (int)(color.x * 1024.0f);
    heatmap_idx = clamp(heatmap_idx, 0, 1023);

    
    uint heatmap_color = colors[heatmap_idx];

    int r = heatmap_color & 0x000000ff;
	int g = heatmap_color & 0x0000ff00;
	int b = heatmap_color & 0x00ff0000;

    r <<= 16;
    b >>= 16;

	render_buffer[pixel_dest] = (r + g + b);//heatmap_color;
}