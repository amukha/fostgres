if(TARGET stress OR TARGET pgtest)
    add_subdirectory(test-views)

    add_custom_command(OUTPUT test-film-t1
            COMMAND fostgres-test fostgres-test-film-t1 -o test-film-t1
                ${CMAKE_CURRENT_SOURCE_DIR}/../Configuration/log-show-all.json
                $<TARGET_SONAME_FILE:fostgres>
                ${CMAKE_CURRENT_SOURCE_DIR}/../Example/films/films.tables.sql
                ${CMAKE_CURRENT_SOURCE_DIR}/films/view.film-slug.json
                ${CMAKE_CURRENT_SOURCE_DIR}/films/film.t1.fg
            MAIN_DEPENDENCY films/film.t1.fg
            DEPENDS
                ../Example/films/films.tables.sql
                fostgres
                fostgres-test
                films/tags.csj
                films/tags-extra-column.csj
                films/tags-short-tag.csj
                films/tags-tab.csj
                films/view.film-slug.json
                films/film.t1.fg
        )

    add_custom_command(OUTPUT test-films
            COMMAND fostgres-test fostgres-test-films -o test-films
                ${CMAKE_CURRENT_SOURCE_DIR}/../Configuration/log-show-all.json
                $<TARGET_SONAME_FILE:fostgres>
                ${CMAKE_CURRENT_SOURCE_DIR}/../Example/films/films.tables.sql
                ${CMAKE_CURRENT_SOURCE_DIR}/films/view.film-slug.json
                ${CMAKE_CURRENT_SOURCE_DIR}/films/films.fg
            MAIN_DEPENDENCY films/films.fg
            DEPENDS
                ../Example/films/films.tables.sql
                fostgres
                fostgres-test
                films/view.film-slug.json
                films/films.csj
                films/films.edit.broken.csj
                films/films.edit.csj
                films/films.fg
        )

        add_custom_command(OUTPUT test-fg-expect
        COMMAND fostgres-test fostgres-test-fg-expect -o test-fg-expect
            ${CMAKE_CURRENT_SOURCE_DIR}/../Configuration/log-show-all.json
            $<TARGET_SONAME_FILE:fostgres>
            $<TARGET_SONAME_FILE:test-views>
            ${CMAKE_CURRENT_SOURCE_DIR}/fg-expect/fg-expect.fg
        MAIN_DEPENDENCY fg-expect/fg-expect.fg
        DEPENDS
            fostgres
            fostgres-test
            fg-expect/fg-expect.fg
    )
    
    ## Because of the way cmake works we need this stuff at the end to
    ## actually make the above commands run when things change.
    add_custom_target(fg-test DEPENDS
            test-fg-expect
            test-films
            test-film-t1
        )
    if(TARGET stress)
        add_dependencies(stress fg-test)
    endif()
    if(TARGET pgtest)
        add_dependencies(pgtest fg-test)
    endif()
endif()
