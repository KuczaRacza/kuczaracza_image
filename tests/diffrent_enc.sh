for q in {1..80..5};
	do for b in {1..40..2};
		do for s in {10..80..5};
			do  ./enc -q "$q" -s "$s" -b "$b" -i ../examples/photo.jpg  -o photo/photo"$b"b"$s"s"$q"q.kczi ;
			./dec  -i photo/photo"$b"b"$s"s"$q"q.kczi -o photo/photo"$b"b"$s"s"$q"q.bmp ;
			convert   photo/photo"$b"b"$s"s"$q"q.bmp  photo/photo"$b"b"$s"s"$q"q.webp;
			rm photo/photo"$b"b"$s"s"$q"q.bmp;

			./enc -q "$q" -s "$s" -b "$b" -i ../examples/anime.jpg  -o anime/anime"$b"b"$s"s"$q"q.kczi ;
			./dec photo -i anime/anime"$b"b"$s"s"$q"q.kczi -o anime/anime"$b"b"$s"s"$q"q.bmp;
			convert anime/anime"$b"b"$s"s"$q"q.bmp  anime/anime"$b"b"$s"s"$q"q.webp;
			rm anime/anime"$b"b"$s"s"$q"q.bmp;
		done;
	done;
done;