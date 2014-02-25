{
    'targets' : [{
        'target_name'  : 'observer',
        'type'         : 'executable',
        'include_dirs' : [
            'src',
        ],
        'sources'      : [
            'src/main.c',
            'src/observer.c',
        ],
        'conditions'   : [
            ['OS=="linux"', {
                'ldflags' : [
                    '-pthread',
                ],
            }],
        ],
    }],
}
